#pragma once

#include <string>
#include <optional>

namespace trading::ports::output {

/**
 * @brief Результат валидации токена
 */
struct TokenValidationResult {
    bool valid = false;
    std::string message;
    std::string userId;
    std::string accountId;
};

/**
 * @brief Интерфейс клиента к Auth Service
 * 
 * Используется для валидации токенов и извлечения account_id.
 */
class IAuthClient {
public:
    virtual ~IAuthClient() = default;

    /**
     * @brief Валидировать access token
     * @param token Access token
     * @return Результат валидации с user_id и account_id
     */
    virtual TokenValidationResult validateAccessToken(const std::string& token) = 0;

    /**
     * @brief Извлечь account_id из токена
     * @param token Access token
     * @return account_id или nullopt если токен невалидный
     */
    virtual std::optional<std::string> getAccountIdFromToken(const std::string& token) = 0;
};

} // namespace trading::ports::output
