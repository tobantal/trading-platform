#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include <nlohmann/json.hpp>
#include <memory>

namespace auth::adapters::primary {

class AddAccountHandler : public IHttpHandler {
public:
    AddAccountHandler(
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

        try {
            auto body = nlohmann::json::parse(req.getBody());
            
            ports::input::CreateAccountRequest request;
            request.name = body.value("name", "");
            request.tinkoffToken = body.value("tinkoff_token", "");
            
            std::string typeStr = body.value("type", "SANDBOX");
            request.type = domain::parseAccountType(typeStr);

            if (request.name.empty()) {
                sendError(res, 400, "name is required");
                return;
            }

            auto account = accountService_->createAccount(validation.userId, request);

            nlohmann::json response;
            response["account_id"] = account.accountId;
            response["name"] = account.name;
            response["type"] = domain::toString(account.type);

            res.setStatus(201);
            res.setHeader("Content-Type", "application/json");
            res.setBody(response.dump());

        } catch (const std::exception& e) {
            sendError(res, 400, e.what());
        }
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
