#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include "domain/enums/AccountType.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief Хэндлер получения списка аккаунтов пользователя
 * 
 * Endpoint: GET /api/v1/accounts
 * 
 * Возвращает все брокерские аккаунты, привязанные к пользователю.
 * 
 * Request:
 *   Authorization: Bearer <session_token>
 * 
 * Response (200 OK):
 *   {
 *     "accounts": [
 *       {
 *         "id": "acc-87654321",
 *         "name": "Мой счёт",
 *         "type": "SANDBOX",
 *         "active": true
 *       },
 *       {
 *         "id": "acc-12345678",
 *         "name": "Реальный счёт",
 *         "type": "PRODUCTION",
 *         "active": true
 *       }
 *     ]
 *   }
 * 
 * Errors:
 *   401 Unauthorized — отсутствует или невалидный session token
 */
class GetAccountsHandler : public IHttpHandler {
public:
    /**
     * @brief Конструктор
     * 
     * @param authService Сервис аутентификации (для валидации токена)
     * @param accountService Сервис аккаунтов
     */
    GetAccountsHandler(
        std::shared_ptr<ports::input::IAuthService> authService,
        std::shared_ptr<ports::input::IAccountService> accountService
    ) : authService_(std::move(authService))
      , accountService_(std::move(accountService))
    {
        std::cout << "[GetAccountsHandler] Created" << std::endl;
    }

    /**
     * @brief Обработать запрос получения аккаунтов
     */
    void handle(IRequest& req, IResponse& res) override {
        // Извлекаем session token
        auto sessionToken = extractBearerToken(req);
        if (!sessionToken) {
            sendError(res, 401, "Session token required. Use Authorization: Bearer <session_token>");
            return;
        }

        // Проверяем токен и извлекаем userId
        if (!authService_->isValidSessionToken(*sessionToken)) {
            sendError(res, 401, "Invalid or expired session token");
            return;
        }

        auto userId = authService_->getUserIdFromToken(*sessionToken);
        if (!userId) {
            sendError(res, 401, "Cannot extract user from token");
            return;
        }

        // Получаем список аккаунтов
        auto accounts = accountService_->getUserAccounts(*userId);

        // Формируем ответ
        nlohmann::json accountsJson = nlohmann::json::array();
        for (const auto& acc : accounts) {
            accountsJson.push_back({
                {"id", acc.id},
                {"name", acc.name},
                {"type", domain::toString(acc.type)},
                {"active", acc.active}
            });
        }

        nlohmann::json response;
        response["accounts"] = accountsJson;

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

private:
    std::shared_ptr<ports::input::IAuthService> authService_;
    std::shared_ptr<ports::input::IAccountService> accountService_;

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
