#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief HTTP Handler для аутентификации
 * 
 * Endpoints:
 * - POST /api/v1/auth/login
 * - POST /api/v1/auth/validate
 */
class AuthHandler : public IHttpHandler
{
public:
    explicit AuthHandler(std::shared_ptr<ports::input::IAuthService> authService)
        : authService_(std::move(authService))
    {
        std::cout << "[AuthHandler] Created" << std::endl;
    }

    void handle(IRequest& req, IResponse& res) override
    {
        std::string path = req.getPath();
        
        if (path == "/api/v1/auth/login") {
            handleLogin(req, res);
        } else if (path == "/api/v1/auth/validate") {
            handleValidate(req, res);
        } else {
            res.setStatus(404);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Not found"})");
        }
    }

private:
    std::shared_ptr<ports::input::IAuthService> authService_;

    /**
     * @brief Обрабатывает запрос логина.
     */
    void handleLogin(IRequest& req, IResponse& res)
    {
        try {
            auto body = nlohmann::json::parse(req.getBody());
            std::string username = body.value("username", "");

            if (username.empty()) {
                res.setStatus(400);
                res.setHeader("Content-Type", "application/json");
                res.setBody(R"({"error": "Username is required"})");
                return;
            }

            auto result = authService_->login(username);

            nlohmann::json response;
            if (result.success) {
                response["access_token"] = result.accessToken;
                response["token_type"] = result.tokenType;
                response["expires_in"] = result.expiresIn;
                res.setStatus(200);
            } else {
                response["error"] = result.error;
                res.setStatus(401);
            }
            
            res.setHeader("Content-Type", "application/json");
            res.setBody(response.dump());
            
        } catch (const nlohmann::json::exception& e) {
            res.setStatus(400);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Invalid JSON"})");
        }
    }

    /**
     * @brief Обрабатывает запрос валидации токена.
     */
    void handleValidate(IRequest& req, IResponse& res)
    {
        try {
            auto body = nlohmann::json::parse(req.getBody());
            std::string token = body.value("token", "");

            if (token.empty()) {
                res.setStatus(400);
                res.setHeader("Content-Type", "application/json");
                res.setBody(R"({"error": "Token is required"})");
                return;
            }

            bool valid = authService_->validateToken(token);

            nlohmann::json response;
            response["valid"] = valid;
            
            if (valid) {
                auto userId = authService_->getUserIdFromToken(token);
                auto username = authService_->getUsernameFromToken(token);
                if (userId) response["user_id"] = *userId;
                if (username) response["username"] = *username;
            }

            res.setStatus(200);
            res.setHeader("Content-Type", "application/json");
            res.setBody(response.dump());
            
        } catch (const nlohmann::json::exception& e) {
            res.setStatus(400);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Invalid JSON"})");
        }
    }
};

} // namespace trading::adapters::primary
