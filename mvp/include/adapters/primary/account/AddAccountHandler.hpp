#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include "domain/AccountRequest.hpp"
#include "domain/enums/AccountType.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief Хэндлер добавления (привязки) брокерского аккаунта
 * 
 * Endpoint: POST /api/v1/accounts
 * 
 * Привязывает брокерский аккаунт к пользователю.
 * 
 * Request:
 *   Authorization: Bearer <session_token>
 *   {
 *     "name": "Мой счёт",
 *     "type": "SANDBOX",
 *     "broker_token": "t.xxxxx"
 *   }
 * 
 * Response (201 Created):
 *   {
 *     "account": {
 *       "id": "acc-87654321",
 *       "name": "Мой счёт",
 *       "type": "SANDBOX",
 *       "active": true
 *     }
 *   }
 * 
 * Errors:
 *   400 Bad Request — невалидный JSON, отсутствуют обязательные поля
 *   401 Unauthorized — отсутствует или невалидный session token
 *   422 Unprocessable Entity — невалидный тип аккаунта
 */
class AddAccountHandler : public IHttpHandler {
public:
    /**
     * @brief Конструктор
     * 
     * @param authService Сервис аутентификации (для валидации токена)
     * @param accountService Сервис аккаунтов
     */
    AddAccountHandler(
        std::shared_ptr<ports::input::IAuthService> authService,
        std::shared_ptr<ports::input::IAccountService> accountService
    ) : authService_(std::move(authService))
      , accountService_(std::move(accountService))
    {
        std::cout << "[AddAccountHandler] Created" << std::endl;
    }

    /**
     * @brief Обработать запрос добавления аккаунта
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

        // Парсинг JSON
        nlohmann::json body;
        try {
            body = nlohmann::json::parse(req.getBody());
        } catch (const std::exception& e) {
            sendError(res, 400, "Invalid JSON");
            return;
        }

        // Извлечение полей
        std::string name = body.value("name", "");
        std::string typeStr = body.value("type", "");
        std::string brokerToken = body.value("broker_token", "");

        if (name.empty()) {
            sendError(res, 400, "Account name is required");
            return;
        }

        if (typeStr.empty()) {
            sendError(res, 400, "Account type is required");
            return;
        }

        if (brokerToken.empty()) {
            sendError(res, 400, "Broker token is required");
            return;
        }

        // Парсинг типа аккаунта
        domain::AccountType accountType;
        try {
            accountType = domain::accountTypeFromString(typeStr);
        } catch (const std::exception& e) {
            sendError(res, 422, "Invalid account type. Use SANDBOX or PRODUCTION");
            return;
        }

        // Создание запроса
        domain::AccountRequest request;
        request.name = name;
        request.type = accountType;
        request.accessToken = brokerToken;

        // Добавление аккаунта
        auto account = accountService_->addAccount(*userId, request);

        // Формируем ответ
        nlohmann::json response;
        response["account"] = {
            {"id", account.id},
            {"name", account.name},
            {"type", domain::toString(account.type)},
            {"active", account.active}
        };

        res.setStatus(201);
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
