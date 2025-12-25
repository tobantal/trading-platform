#pragma once

#include "ports/input/IAuthService.hpp"
#include <http_server/IRequestHandler.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <sstream>

namespace trading::adapters::primary {

/**
 * @brief REST контроллер аутентификации
 * 
 * Endpoints:
 * - POST /api/v1/auth/login - логин пользователя
 * - POST /api/v1/auth/validate - валидация токена
 */
class AuthController : public microservice::IRequestHandler {
public:
    explicit AuthController(std::shared_ptr<ports::input::IAuthService> authService)
        : authService_(std::move(authService))
    {}

    microservice::Response handle(const microservice::Request& request) override {
        // POST /api/v1/auth/login
        if (request.method == "POST" && request.path == "/api/v1/auth/login") {
            return handleLogin(request);
        }
        
        // POST /api/v1/auth/validate
        if (request.method == "POST" && request.path == "/api/v1/auth/validate") {
            return handleValidate(request);
        }

        return notFound();
    }

private:
    std::shared_ptr<ports::input::IAuthService> authService_;

    microservice::Response handleLogin(const microservice::Request& request) {
        try {
            auto body = nlohmann::json::parse(request.body);
            std::string username = body.value("username", "");

            if (username.empty()) {
                return badRequest("Username is required");
            }

            auto result = authService_->login(username);

            nlohmann::json response;
            if (result.success) {
                response["access_token"] = result.accessToken;
                response["token_type"] = result.tokenType;
                response["expires_in"] = result.expiresIn;
                return jsonResponse(200, response);
            } else {
                response["error"] = result.error;
                return jsonResponse(401, response);
            }
        } catch (const nlohmann::json::exception& e) {
            return badRequest("Invalid JSON: " + std::string(e.what()));
        }
    }

    microservice::Response handleValidate(const microservice::Request& request) {
        try {
            auto body = nlohmann::json::parse(request.body);
            std::string token = body.value("token", "");

            if (token.empty()) {
                return badRequest("Token is required");
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

            return jsonResponse(200, response);
        } catch (const nlohmann::json::exception& e) {
            return badRequest("Invalid JSON: " + std::string(e.what()));
        }
    }

    microservice::Response jsonResponse(int status, const nlohmann::json& body) {
        microservice::Response resp;
        resp.status = status;
        resp.headers["Content-Type"] = "application/json";
        resp.body = body.dump();
        return resp;
    }

    microservice::Response badRequest(const std::string& message) {
        nlohmann::json body;
        body["error"] = message;
        return jsonResponse(400, body);
    }

    microservice::Response notFound() {
        nlohmann::json body;
        body["error"] = "Not found";
        return jsonResponse(404, body);
    }
};

} // namespace trading::adapters::primary
