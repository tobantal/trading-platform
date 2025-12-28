#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief Хэндлер регистрации пользователя
 * 
 * Endpoint: POST /api/v1/auth/register
 * 
 * Request:
 *   {
 *     "username": "trader1",
 *     "password": "secret123"
 *   }
 * 
 * Response (201 Created):
 *   {
 *     "user_id": "user-12345678",
 *     "message": "User registered successfully"
 *   }
 * 
 * Errors:
 *   400 Bad Request — невалидный JSON, отсутствуют поля
 *   409 Conflict — username уже занят
 *   422 Unprocessable Entity — невалидный формат username или пароля
 */
class RegisterHandler : public IHttpHandler {
public:
    /**
     * @brief Конструктор
     * 
     * @param authService Сервис аутентификации
     */
    explicit RegisterHandler(
        std::shared_ptr<ports::input::IAuthService> authService
    ) : authService_(std::move(authService))
    {
        std::cout << "[RegisterHandler] Created" << std::endl;
    }

    /**
     * @brief Обработать запрос регистрации
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

        // Регистрация
        auto result = authService_->registerUser(username, password);

        if (!result.success) {
            // Определяем код ошибки по тексту
            int status = 422;
            if (result.error.find("already exists") != std::string::npos) {
                status = 409;
            }
            sendError(res, status, result.error);
            return;
        }

        // Успешный ответ
        nlohmann::json response;
        response["user_id"] = result.userId;
        response["message"] = "User registered successfully";

        res.setStatus(201);
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
