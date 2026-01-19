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
        auto token = req.getBearerToken().value_or("");
        if (token.empty()) {
            sendError(res, 401, "Authorization required");
            return;
        }

        auto validation = authService_->validateSessionToken(token);
        if (!validation.valid) {
            sendError(res, 401, validation.message);
            return;
        }

        std::string accountId = req.getPathParam(0).value_or("");

        if (!accountService_->isAccountOwner(validation.userId, accountId)) {
            sendError(res, 403, "Not your account");
            return;
        }

        bool deleted = accountService_->deleteAccount(accountId);

        nlohmann::json response;
        response["deleted"] = deleted;

        res.setResult(deleted ? 200 : 404, "application/json", response.dump());
    }

private:
    std::shared_ptr<ports::input::IAuthService> authService_;
    std::shared_ptr<ports::input::IAccountService> accountService_;

    void sendError(IResponse& res, int status, const std::string& msg) {
        res.setResult(status, "application/json", "{\"error\": \"" + msg + "\"}");
    }
};

} // namespace auth::adapters::primary
