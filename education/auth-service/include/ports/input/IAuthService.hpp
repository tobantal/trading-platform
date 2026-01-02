#pragma once

#include "domain/User.hpp"
#include <string>
#include <optional>

namespace auth::ports::input {

/**
 * @brief Результат регистрации
 */
struct RegisterResult {
    bool success;
    std::string userId;
    std::string message;
};

/**
 * @brief Результат логина
 */
struct LoginResult {
    bool success;
    std::string sessionToken;
    std::string userId;
    std::string message;
};

/**
 * @brief Результат валидации токена
 */
struct ValidateResult {
    bool valid;
    std::string userId;
    std::string accountId;  // Для access token
    std::string message;
};

/**
 * @brief Интерфейс сервиса аутентификации
 */
class IAuthService {
public:
    virtual ~IAuthService() = default;

    /**
     * @brief Регистрация нового пользователя
     */
    virtual RegisterResult registerUser(
        const std::string& username,
        const std::string& email,
        const std::string& password
    ) = 0;

    /**
     * @brief Логин пользователя
     */
    virtual LoginResult login(
        const std::string& username,
        const std::string& password
    ) = 0;

    /**
     * @brief Выход (инвалидация токена)
     */
    virtual bool logout(const std::string& sessionToken) = 0;

    /**
     * @brief Валидация session token
     */
    virtual ValidateResult validateSessionToken(const std::string& token) = 0;

    /**
     * @brief Валидация access token
     */
    virtual ValidateResult validateAccessToken(const std::string& token) = 0;

    /**
     * @brief Создать access token для выбранного аккаунта
     */
    virtual std::optional<std::string> createAccessToken(
        const std::string& sessionToken,
        const std::string& accountId
    ) = 0;
};

} // namespace auth::ports::input
