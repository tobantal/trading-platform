#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include <nlohmann/json.hpp>
#include <memory>

namespace auth::adapters::primary {

class DeleteAccountHandler : public IHttpHandler {
public:
    DeleteAccountHandler(
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

        // Extract account_id from path: /api/v1/accounts/{id}
        std::string path = req.getPath();
        size_t lastSlash = path.rfind('/');
        if (lastSlash == std::string::npos) {
            sendError(res, 400, "Invalid path");
            return;
        }
        std::string accountId = path.substr(lastSlash + 1);

        if (!accountService_->isAccountOwner(validation.userId, accountId)) {
            sendError(res, 403, "Not your account");
            return;
        }

        bool deleted = accountService_->deleteAccount(accountId);

        nlohmann::json response;
        response["deleted"] = deleted;

        res.setStatus(deleted ? 200 : 404);
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
