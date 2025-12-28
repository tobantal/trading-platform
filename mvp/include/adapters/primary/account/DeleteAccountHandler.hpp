#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief Хэндлер удаления (отвязки) брокерского аккаунта
 * 
 * Endpoint: DELETE /api/v1/accounts/{account_id}
 * 
 * Отвязывает брокерский аккаунт от пользователя.
 * 
 * Request:
 *   Authorization: Bearer <session_token>
 *   DELETE /api/v1/accounts/acc-87654321
 * 
 * Response (200 OK):
 *   {
 *     "message": "Account deleted successfully"
 *   }
 * 
 * Errors:
 *   401 Unauthorized — отсутствует или невалидный session token
 *   403 Forbidden — аккаунт не принадлежит пользователю
 *   404 Not Found — аккаунт не найден
 */
class DeleteAccountHandler : public IHttpHandler {
public:
    /**
     * @brief Конструктор
     * 
     * @param authService Сервис аутентификации (для валидации токена)
     * @param accountService Сервис аккаунтов
     */
    DeleteAccountHandler(
        std::shared_ptr<ports::input::IAuthService> authService,
        std::shared_ptr<ports::input::IAccountService> accountService
    ) : authService_(std::move(authService))
      , accountService_(std::move(accountService))
    {
        std::cout << "[DeleteAccountHandler] Created" << std::endl;
    }

    /**
     * @brief Обработать запрос удаления аккаунта
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

        // Извлекаем accountId из пути
        // Путь: /api/v1/accounts/{account_id}
        std::string accountId = extractAccountIdFromPath(req.getPath());
        if (accountId.empty()) {
            sendError(res, 400, "Account ID is required in path");
            return;
        }

        // Проверяем, что аккаунт существует и принадлежит пользователю
        auto accountOpt = accountService_->getAccountById(accountId);
        if (!accountOpt) {
            sendError(res, 404, "Account not found");
            return;
        }

        if (accountOpt->userId != *userId) {
            sendError(res, 403, "Account does not belong to user");
            return;
        }

        // Удаляем аккаунт
        bool deleted = accountService_->deleteAccount(accountId);
        if (!deleted) {
            sendError(res, 500, "Failed to delete account");
            return;
        }

        // Формируем ответ
        nlohmann::json response;
        response["message"] = "Account deleted successfully";

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

private:
    std::shared_ptr<ports::input::IAuthService> authService_;
    std::shared_ptr<ports::input::IAccountService> accountService_;

    /**
     * @brief Извлечь accountId из пути запроса
     * 
     * @param path Путь вида "/api/v1/accounts/acc-12345678"
     * @return accountId или пустая строка
     */
    std::string extractAccountIdFromPath(const std::string& path) {
        // Путь: /api/v1/accounts/{account_id}
        const std::string prefix = "/api/v1/accounts/";
        
        if (path.find(prefix) != 0) {
            return "";
        }
        
        std::string accountId = path.substr(prefix.length());
        
        // Убираем query string если есть
        size_t queryPos = accountId.find('?');
        if (queryPos != std::string::npos) {
            accountId = accountId.substr(0, queryPos);
        }
        
        return accountId;
    }

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
