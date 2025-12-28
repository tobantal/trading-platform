#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief Хэндлер выхода из системы
 * 
 * Endpoint: POST /api/v1/auth/logout
 * 
 * Инвалидирует токен. После вызова токен становится невалидным.
 * 
 * Request:
 *   Authorization: Bearer <token>
 *   (тело опционально)
 * 
 *   Опционально можно выйти из всех сессий:
 *   {
 *     "logout_all": true
 *   }
 * 
 * Response (200 OK):
 *   {
 *     "message": "Logged out successfully"
 *   }
 * 
 *   или (при logout_all):
 *   {
 *     "message": "Logged out from all sessions"
 *   }
 * 
 * Errors:
 *   401 Unauthorized — отсутствует токен
 * 
 * @note Принимает как session, так и access token.
 * @note Если токен уже невалиден — возвращает 200 OK (идемпотентность).
 */
class LogoutHandler : public IHttpHandler {
public:
    /**
     * @brief Конструктор
     * 
     * @param authService Сервис аутентификации
     */
    explicit LogoutHandler(
        std::shared_ptr<ports::input::IAuthService> authService
    ) : authService_(std::move(authService))
    {
        std::cout << "[LogoutHandler] Created" << std::endl;
    }

    /**
     * @brief Обработать запрос выхода
     */
    void handle(IRequest& req, IResponse& res) override {
        // Извлекаем токен
        auto token = extractBearerToken(req);
        if (!token) {
            sendError(res, 401, "Token required. Use Authorization: Bearer <token>");
            return;
        }

        // Проверяем валидность и получаем userId
        auto validateResult = authService_->validateToken(*token);
        
        // Если токен уже невалиден — считаем успехом (идемпотентность)
        if (!validateResult.valid) {
            sendSuccess(res, "Already logged out or token expired");
            return;
        }

        // Проверяем logout_all
        bool logoutAll = false;
        if (!req.getBody().empty()) {
            try {
                auto body = nlohmann::json::parse(req.getBody());
                logoutAll = body.value("logout_all", false);
            } catch (...) {
                // Игнорируем ошибки парсинга
            }
        }

        if (logoutAll) {
            // Выход из всех сессий
            authService_->logoutAll(validateResult.userId);
            sendSuccess(res, "Logged out from all sessions");
        } else {
            // Выход только из текущей сессии
            authService_->logout(*token);
            sendSuccess(res, "Logged out successfully");
        }
    }

private:
    std::shared_ptr<ports::input::IAuthService> authService_;

    /**
     * @brief Извлечь Bearer токен из заголовка Authorization
     */
    std::optional<std::string> extractBearerToken(const IRequest& req) {
        auto headers = req.getHeaders();
        
        auto it = headers.find("Authorization");
        if (it == headers.end()) {
            it = headers.find("authorization");
        }
        
        if (it == headers.end()) {
            return std::nullopt;
        }

        const std::string& authHeader = it->second;
        
        if (authHeader.size() <= 7 || authHeader.substr(0, 7) != "Bearer ") {
            return std::nullopt;
        }

        return authHeader.substr(7);
    }

    /**
     * @brief Отправить успешный ответ
     */
    void sendSuccess(IResponse& res, const std::string& message) {
        nlohmann::json response;
        response["message"] = message;
        
        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

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
