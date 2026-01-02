#pragma once

#include <string>
#include <optional>

namespace auth::ports::output {

/**
 * @brief Интерфейс провайдера JWT токенов
 */
class IJwtProvider {
public:
    virtual ~IJwtProvider() = default;

    /**
     * @brief Создать session token
     * @param userId ID пользователя
     * @param lifetimeSeconds Время жизни в секундах
     * @return JWT токен
     */
    virtual std::string createSessionToken(
        const std::string& userId,
        int lifetimeSeconds
    ) = 0;

    /**
     * @brief Создать access token (с accountId)
     * @param userId ID пользователя
     * @param accountId ID аккаунта
     * @param lifetimeSeconds Время жизни в секундах
     * @return JWT токен
     */
    virtual std::string createAccessToken(
        const std::string& userId,
        const std::string& accountId,
        int lifetimeSeconds
    ) = 0;

    /**
     * @brief Проверить валидность токена
     */
    virtual bool isValidToken(const std::string& token) = 0;

    /**
     * @brief Извлечь userId из токена
     */
    virtual std::optional<std::string> getUserId(const std::string& token) = 0;

    /**
     * @brief Извлечь accountId из токена (только для access token)
     */
    virtual std::optional<std::string> getAccountId(const std::string& token) = 0;

    /**
     * @brief Инвалидировать токен
     */
    virtual void invalidateToken(const std::string& token) = 0;
};

} // namespace auth::ports::output
