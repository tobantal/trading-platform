#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include <nlohmann/json.hpp>
#include <memory>

namespace auth::adapters::primary {

/**
 * @brief Валидация токена (внутренний API для Trading Service)
 * 
 * POST /api/v1/auth/validate
 * {
 *   "token": "eyJ..."
 * }
 * 
 * Response:
 * {
 *   "valid": true,
 *   "user_id": "user-123",
 *   "account_id": "acc-456"  // для access token
 * }
 */
class ValidateTokenHandler : public IHttpHandler {
public:
    explicit ValidateTokenHandler(
        std::shared_ptr<ports::input::IAuthService> authService
    ) : authService_(std::move(authService)) {}

    void handle(IRequest& req, IResponse& res) override {
        try {
            auto body = nlohmann::json::parse(req.getBody());
            
            std::string token = body.value("token", "");
            std::string tokenType = body.value("type", "session");  // session или access

            if (token.empty()) {
                sendError(res, 400, "token is required");
                return;
            }

            ports::input::ValidateResult result;
            if (tokenType == "access") {
                result = authService_->validateAccessToken(token);
            } else {
                result = authService_->validateSessionToken(token);
            }

            nlohmann::json response;
            response["valid"] = result.valid;
            
            if (result.valid) {
                response["user_id"] = result.userId;
                if (!result.accountId.empty()) {
                    response["account_id"] = result.accountId;
                }
            } else {
                response["message"] = result.message;
            }

            res.setStatus(200);
            res.setHeader("Content-Type", "application/json");
            res.setBody(response.dump());

        } catch (const nlohmann::json::exception& e) {
            sendError(res, 400, "Invalid JSON");
        }
    }

private:
    std::shared_ptr<ports::input::IAuthService> authService_;

    void sendError(IResponse& res, int status, const std::string& message) {
        nlohmann::json error;
        error["error"] = message;
        res.setStatus(status);
        res.setHeader("Content-Type", "application/json");
        res.setBody(error.dump());
    }
};

} // namespace auth::adapters::primary
