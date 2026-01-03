#pragma once

#include "ports/output/IAuthClient.hpp"
#include "settings/AuthClientSettings.hpp"
#include <IHttpClient.hpp>
#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::secondary {

/**
 * @brief HTTP клиент к Auth Service
 * 
 * Вызывает POST /api/v1/auth/validate для валидации токенов.
 */
class HttpAuthClient : public ports::output::IAuthClient {
public:
    HttpAuthClient(
        std::shared_ptr<IHttpClient> httpClient,
        std::shared_ptr<settings::AuthClientSettings> settings
    ) : httpClient_(std::move(httpClient))
      , settings_(std::move(settings))
    {
        std::cout << "[HttpAuthClient] Created, target: "
                  << settings_->getHost() << ":" << settings_->getPort() << std::endl;
    }

    ports::output::TokenValidationResult validateAccessToken(const std::string& token) override {
        ports::output::TokenValidationResult result;
        
        try {
            nlohmann::json requestBody = {
                {"token", token},
                {"type", "access"}
            };

            SimpleRequest request(
                "POST",
                "/api/v1/auth/validate",
                requestBody.dump(),
                settings_->getHost(),
                settings_->getPort(),
                {{"Content-Type", "application/json"}}
            );

            SimpleResponse response;
            httpClient_->send(request, response);

            if (response.getStatus() == 200) {
                auto responseBody = nlohmann::json::parse(response.getBody());
                
                result.valid = responseBody.value("valid", false);
                result.userId = responseBody.value("user_id", "");
                result.accountId = responseBody.value("account_id", "");
                result.message = responseBody.value("message", "");
            } else {
                result.valid = false;
                result.message = "Auth service returned " + std::to_string(response.getStatus());
            }

        } catch (const std::exception& e) {
            std::cerr << "[HttpAuthClient] Error: " << e.what() << std::endl;
            result.valid = false;
            result.message = std::string("Auth service error: ") + e.what();
        }

        return result;
    }

    std::optional<std::string> getAccountIdFromToken(const std::string& token) override {
        auto result = validateAccessToken(token);
        
        if (result.valid && !result.accountId.empty()) {
            return result.accountId;
        }
        
        return std::nullopt;
    }

private:
    std::shared_ptr<IHttpClient> httpClient_;
    std::shared_ptr<settings::AuthClientSettings> settings_;
};

} // namespace trading::adapters::secondary
