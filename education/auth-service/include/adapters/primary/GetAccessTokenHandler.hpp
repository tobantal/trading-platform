#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include <nlohmann/json.hpp>
#include <memory>

namespace auth::adapters::primary {

/**
 * @brief Handler для получения access_token
 *
 * POST /api/v1/auth/access-token
 * Header: Authorization: Bearer {session_token}
 * Body: {"account_id": "acc-xxx"}
 * Response: {"access_token": "eyJ..."}
 *
 * Flow:
 * 1. Валидирует session_token
 * 2. Проверяет что account принадлежит user
 * 3. Генерирует access_token с user_id + account_id
 */
class GetAccessTokenHandler : public IHttpHandler {
public:
    GetAccessTokenHandler(
        std::shared_ptr<ports::input::IAuthService> authService,
        std::shared_ptr<ports::input::IAccountService> accountService
    ) : authService_(std::move(authService))
      , accountService_(std::move(accountService)) {}

    void handle(IRequest& req, IResponse& res) override {
        // 1. Извлекаем session_token из заголовка
        auto sessionToken = extractBearerToken(req);
        if (!sessionToken) {
            sendError(res, 401, "Authorization required. Use session_token in Bearer header.");
            return;
        }

        // 2. Валидируем session_token и получаем user_id
        auto validation = authService_->validateSessionToken(*sessionToken);
        if (!validation.valid) {
            sendError(res, 401, validation.message);
            return;
        }

        // 3. Парсим body и получаем account_id
        std::string accountId;
        try {
            auto body = nlohmann::json::parse(req.getBody());
            accountId = body.value("account_id", "");
            
            if (accountId.empty()) {
                sendError(res, 400, "account_id is required");
                return;
            }
        } catch (const nlohmann::json::exception& e) {
            sendError(res, 400, "Invalid JSON");
            return;
        }

        // 4. Проверяем что account принадлежит этому user
        if (!accountService_->isAccountOwner(validation.userId, accountId)) {
            sendError(res, 403, "Account does not belong to this user");
            return;
        }

        // 5. Создаём access_token
        auto accessToken = authService_->createAccessToken(*sessionToken, accountId);
        if (!accessToken) {
            sendError(res, 500, "Failed to create access token");
            return;
        }

        // 6. Возвращаем access_token
        nlohmann::json response;
        response["access_token"] = *accessToken;
        response["user_id"] = validation.userId;
        response["account_id"] = accountId;
        response["expires_in"] = 3600;  // 1 час
        response["token_type"] = "Bearer";

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

private:
    std::shared_ptr<ports::input::IAuthService> authService_;
    std::shared_ptr<ports::input::IAccountService> accountService_;

    std::optional<std::string> extractBearerToken(IRequest& req) {
        auto headers = req.getHeaders();
        auto it = headers.find("Authorization");
        if (it == headers.end()) return std::nullopt;
        if (it->second.substr(0, 7) != "Bearer ") return std::nullopt;
        return it->second.substr(7);
    }

    void sendError(IResponse& res, int status, const std::string& msg) {
        res.setStatus(status);
        res.setHeader("Content-Type", "application/json");
        res.setBody("{\"error\": \"" + msg + "\"}");
    }
};

} // namespace auth::adapters::primary
