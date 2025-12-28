#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include "domain/enums/AccountType.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief Хэндлер входа в систему
 * 
 * Endpoint: POST /api/v1/auth/login
 * 
 * Request:
 *   {
 *     "username": "trader1",
 *     "password": "secret123"
 *   }
 * 
 * Response (200 OK):
 *   {
 *     "session_token": "eyJhbGci...",
 *     "token_type": "Bearer",
 *     "expires_in": 86400,
 *     "user": {
 *       "id": "user-12345678",
 *       "username": "trader1"
 *     },
 *     "accounts": [
 *       {
 *         "id": "acc-87654321",
 *         "name": "Мой счёт",
 *         "type": "SANDBOX",
 *         "active": true
 *       }
 *     ]
 *   }
 * 
 * Errors:
 *   400 Bad Request — невалидный JSON, отсутствуют поля
 *   401 Unauthorized — пользователь не найден или неверный пароль
 */
class LoginHandler : public IHttpHandler {
public:
    /**
     * @brief Конструктор
     * 
     * @param authService Сервис аутентификации
     */
    explicit LoginHandler(
        std::shared_ptr<ports::input::IAuthService> authService
    ) : authService_(std::move(authService))
    {
        std::cout << "[LoginHandler] Created" << std::endl;
    }

    /**
     * @brief Обработать запрос входа
     */
    void handle(IRequest& req, IResponse& res) override {
        // Парсинг JSON
        nlohmann::json body;
        try {
            body = nlohmann::json::parse(req.getBody());
        } catch (const std::exception& e) {
            sendError(res, 400, "Invalid JSON");
            return;
        }

        // Извлечение полей
        std::string username = body.value("username", "");
        std::string password = body.value("password", "");

        if (username.empty()) {
            sendError(res, 400, "Username is required");
            return;
        }

        if (password.empty()) {
            sendError(res, 400, "Password is required");
            return;
        }

        // Вход
        auto result = authService_->login(username, password);

        if (!result.success) {
            sendError(res, 401, result.error);
            return;
        }

        // Формируем ответ
        nlohmann::json response;
        response["session_token"] = result.sessionToken;
        response["token_type"] = result.tokenType;
        response["expires_in"] = result.expiresIn;

        // Информация о пользователе
        response["user"] = {
            {"id", result.user.id},
            {"username", result.user.username}
        };

        // Список аккаунтов
        nlohmann::json accountsJson = nlohmann::json::array();
        for (const auto& acc : result.accounts) {
            accountsJson.push_back({
                {"id", acc.id},
                {"name", acc.name},
                {"type", domain::toString(acc.type)},
                {"active", acc.active}
            });
        }
        response["accounts"] = accountsJson;

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

private:
    std::shared_ptr<ports::input::IAuthService> authService_;

    /**
     * @brief Отправить ошибку
     */
    void sendError(IResponse& res, int status, const std::string& message) {
        nlohmann::json error;
        error["error"] = message;
        
        res.setStatus(status);
        res.setHeader("Content-Type", "application/json");
        res.setBody(error.dump());
    }
};

} // namespace trading::adapters::primary
