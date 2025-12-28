#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief Хэндлер обновления session token
 * 
 * Endpoint: POST /api/v1/auth/refresh
 * 
 * Выдаёт новый session token с полным временем жизни.
 * Старый токен становится невалидным.
 * 
 * Request:
 *   Authorization: Bearer <session_token>
 *   (тело не требуется)
 * 
 * Response (200 OK):
 *   {
 *     "session_token": "eyJhbGci...",
 *     "token_type": "Bearer",
 *     "expires_in": 86400
 *   }
 * 
 * Errors:
 *   401 Unauthorized — отсутствует, невалидный или не session token
 * 
 * @note Работает ТОЛЬКО с session token.
 * @note Access token обновить нельзя — нужно заново вызвать select-account.
 */
class RefreshTokenHandler : public IHttpHandler {
public:
    /**
     * @brief Конструктор
     * 
     * @param authService Сервис аутентификации
     */
    explicit RefreshTokenHandler(
        std::shared_ptr<ports::input::IAuthService> authService
    ) : authService_(std::move(authService))
    {
        std::cout << "[RefreshTokenHandler] Created" << std::endl;
    }

    /**
     * @brief Обработать запрос обновления токена
     */
    void handle(IRequest& req, IResponse& res) override {
        // Извлекаем session token
        auto sessionToken = extractBearerToken(req);
        if (!sessionToken) {
            sendError(res, 401, "Session token required. Use Authorization: Bearer <session_token>");
            return;
        }

        // Проверяем, что это session token
        if (!authService_->isValidSessionToken(*sessionToken)) {
            sendError(res, 401, "Invalid or expired session token. Access tokens cannot be refreshed.");
            return;
        }

        // Обновление
        auto result = authService_->refreshSession(*sessionToken);

        if (!result.success) {
            sendError(res, 401, result.error);
            return;
        }

        // Формируем ответ
        nlohmann::json response;
        response["session_token"] = result.sessionToken;
        response["token_type"] = "Bearer";
        response["expires_in"] = result.expiresIn;

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
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
