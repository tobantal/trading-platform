#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief Хэндлер валидации токена
 * 
 * Endpoint: POST /api/v1/auth/validate
 * 
 * Проверяет валидность токена (session или access) и возвращает информацию о нём.
 * 
 * Request:
 *   {
 *     "token": "eyJhbGci..."
 *   }
 * 
 * Response (200 OK) — токен валиден:
 *   {
 *     "valid": true,
 *     "token_type": "access",
 *     "user_id": "user-12345678",
 *     "username": "trader1",
 *     "account_id": "acc-87654321",  // только для access token
 *     "remaining_seconds": 3540
 *   }
 * 
 * Response (200 OK) — токен невалиден:
 *   {
 *     "valid": false,
 *     "error": "Invalid or expired token"
 *   }
 * 
 * Errors:
 *   400 Bad Request — невалидный JSON, отсутствует token
 * 
 * @note Всегда возвращает 200 OK. Поле "valid" указывает на результат.
 */
class ValidateTokenHandler : public IHttpHandler {
public:
    /**
     * @brief Конструктор
     * 
     * @param authService Сервис аутентификации
     */
    explicit ValidateTokenHandler(
        std::shared_ptr<ports::input::IAuthService> authService
    ) : authService_(std::move(authService))
    {
        std::cout << "[ValidateTokenHandler] Created" << std::endl;
    }

    /**
     * @brief Обработать запрос валидации
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

        // Извлечение токена
        std::string token = body.value("token", "");
        if (token.empty()) {
            sendError(res, 400, "Token is required");
            return;
        }

        // Валидация
        auto result = authService_->validateToken(token);

        // Формируем ответ
        nlohmann::json response;
        response["valid"] = result.valid;

        if (result.valid) {
            response["token_type"] = result.tokenType;
            response["user_id"] = result.userId;
            response["username"] = result.username;
            response["remaining_seconds"] = result.remainingSeconds;
            
            if (result.accountId) {
                response["account_id"] = *result.accountId;
            }
        } else {
            response["error"] = result.error;
        }

        // Всегда 200, поле valid указывает результат
        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

private:
    std::shared_ptr<ports::input::IAuthService> authService_;

    /**
     * @brief Отправить ошибку (для ошибок парсинга)
     */
    void sendError(IResponse& res, int status, const std::string& message) {
        nlohmann::json error;
        error["valid"] = false;
        error["error"] = message;
        
        res.setStatus(status);
        res.setHeader("Content-Type", "application/json");
        res.setBody(error.dump());
    }
};

} // namespace trading::adapters::primary
