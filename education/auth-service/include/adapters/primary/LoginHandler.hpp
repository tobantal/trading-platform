#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include <nlohmann/json.hpp>
#include <memory>

namespace auth::adapters::primary {

/**
 * @brief Логин пользователя
 * 
 * POST /api/v1/auth/login
 * {
 *   "username": "john",
 *   "password": "secret123"
 * }
 * 
 * Response:
 * {
 *   "session_token": "eyJ...",
 *   "user_id": "user-123"
 * }
 */
class LoginHandler : public IHttpHandler {
public:
    explicit LoginHandler(
        std::shared_ptr<ports::input::IAuthService> authService
    ) : authService_(std::move(authService)) {}

    void handle(IRequest& req, IResponse& res) override {
        try {
            auto body = nlohmann::json::parse(req.getBody());
            
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");

            if (username.empty() || password.empty()) {
                sendError(res, 400, "username and password are required");
                return;
            }

            auto result = authService_->login(username, password);

            if (!result.success) {
                sendError(res, 401, result.message);
                return;
            }

            nlohmann::json response;
            response["session_token"] = result.sessionToken;
            response["user_id"] = result.userId;
            response["message"] = result.message;

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
