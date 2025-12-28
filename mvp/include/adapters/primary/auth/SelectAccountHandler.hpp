#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include "domain/enums/AccountType.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief Хэндлер выбора аккаунта
 * 
 * Endpoint: POST /api/v1/auth/select-account
 * 
 * Выбирает аккаунт для работы и возвращает access_token.
 * Access token используется для всех торговых операций.
 * 
 * Request:
 *   Authorization: Bearer <session_token>
 *   {
 *     "account_id": "acc-87654321"
 *   }
 * 
 * Response (200 OK):
 *   {
 *     "access_token": "eyJhbGci...",
 *     "token_type": "Bearer",
 *     "expires_in": 3600,
 *     "account": {
 *       "id": "acc-87654321",
 *       "name": "Мой счёт",
 *       "type": "SANDBOX",
 *       "active": true
 *     }
 *   }
 * 
 * Errors:
 *   400 Bad Request — невалидный JSON, отсутствует account_id
 *   401 Unauthorized — отсутствует или невалидный session token
 *   403 Forbidden — аккаунт не принадлежит пользователю
 *   404 Not Found — аккаунт не найден
 */
class SelectAccountHandler : public IHttpHandler {
public:
    /**
     * @brief Конструктор
     * 
     * @param authService Сервис аутентификации
     */
    explicit SelectAccountHandler(
        std::shared_ptr<ports::input::IAuthService> authService
    ) : authService_(std::move(authService))
    {
        std::cout << "[SelectAccountHandler] Created" << std::endl;
    }

    /**
     * @brief Обработать запрос выбора аккаунта
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
            sendError(res, 401, "Invalid or expired session token");
            return;
        }

        // Парсинг JSON
        nlohmann::json body;
        try {
            body = nlohmann::json::parse(req.getBody());
        } catch (const std::exception& e) {
            sendError(res, 400, "Invalid JSON");
            return;
        }

        // Извлечение account_id
        std::string accountId = body.value("account_id", "");
        if (accountId.empty()) {
            sendError(res, 400, "account_id is required");
            return;
        }

        // Выбор аккаунта
        auto result = authService_->selectAccount(*sessionToken, accountId);

        if (!result.success) {
            int status = 400;
            if (result.error.find("not found") != std::string::npos) {
                status = 404;
            } else if (result.error.find("not belong") != std::string::npos) {
                status = 403;
            } else if (result.error.find("not active") != std::string::npos) {
                status = 403;
            }
            sendError(res, status, result.error);
            return;
        }

        // Формируем ответ
        nlohmann::json response;
        response["access_token"] = result.accessToken;
        response["token_type"] = result.tokenType;
        response["expires_in"] = result.expiresIn;

        response["account"] = {
            {"id", result.account.id},
            {"name", result.account.name},
            {"type", domain::toString(result.account.type)},
            {"active", result.account.active}
        };

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

private:
    std::shared_ptr<ports::input::IAuthService> authService_;

    /**
     * @brief Извлечь Bearer токен из заголовка Authorization
     * 
     * @param req HTTP запрос
     * @return Токен или nullopt если заголовок отсутствует/невалиден
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
        
        // Формат: "Bearer <token>"
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
