#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include <nlohmann/json.hpp>
#include <memory>

namespace auth::adapters::primary {

class GetAccountsHandler : public IHttpHandler {
public:
    GetAccountsHandler(
        std::shared_ptr<ports::input::IAuthService> authService,
        std::shared_ptr<ports::input::IAccountService> accountService
    ) : authService_(std::move(authService))
      , accountService_(std::move(accountService)) {}

    void handle(IRequest& req, IResponse& res) override {
        auto token = extractBearerToken(req);
        if (!token) {
            sendError(res, 401, "Authorization required");
            return;
        }

        auto validation = authService_->validateSessionToken(*token);
        if (!validation.valid) {
            sendError(res, 401, validation.message);
            return;
        }

        auto accounts = accountService_->getUserAccounts(validation.userId);

        nlohmann::json response;
        response["accounts"] = nlohmann::json::array();
        for (const auto& acc : accounts) {
            nlohmann::json item;
            item["account_id"] = acc.accountId;
            item["name"] = acc.name;
            item["type"] = domain::toString(acc.type);
            response["accounts"].push_back(item);
        }

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

private:
    std::shared_ptr<ports::input::IAuthService> authService_;
    std::shared_ptr<ports::input::IAccountService> accountService_;

    std::optional<std::string> extractBearerToken(IRequest& req) {
        auto headers = req.getHeaders();
        auto it = headers.find("Authorization");
        if (it == headers.end()) return std::nullopt;
        if (it->second.substr(0, 7) != "Bearer ") return std::nullopt;
        return it->second.substr(7);
    }

    void sendError(IResponse& res, int status, const std::string& msg) {
        res.setStatus(status);
        res.setHeader("Content-Type", "application/json");
        res.setBody("{\"error\": \"" + msg + "\"}");
    }
};

} // namespace auth::adapters::primary
