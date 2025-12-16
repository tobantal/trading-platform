#pragma once

#include <string>
#include <optional>

namespace trading::ports::input {

/**
 * @brief Интерфейс сервиса авторизации
 * 
 * Input Port для работы с аутентификацией пользователей.
 * Адаптируется к fake-jwt-server (MVP) или ZITADEL (Education).
 */
class IAuthService {
public:
    virtual ~IAuthService() = default;

    /**
     * @brief Результат логина
     */
    struct LoginResult {
        std::string accessToken;    ///< JWT токен
        std::string tokenType;      ///< "Bearer"
        int expiresIn;              ///< Время жизни в секундах
        bool success;               ///< Успешность операции
        std::string error;          ///< Сообщение об ошибке
    };

    /**
     * @brief Выполнить логин пользователя
     * 
     * @param username Имя пользователя
     * @return LoginResult с токеном или ошибкой
     * 
     * @note В MVP пароль не проверяется, создаётся/находится пользователь по username
     */
    virtual LoginResult login(const std::string& username) = 0;

    /**
     * @brief Валидировать JWT токен
     * 
     * @param token JWT токен
     * @return true если токен валиден и не истёк
     */
    virtual bool validateToken(const std::string& token) = 0;

    /**
     * @brief Извлечь userId из токена
     * 
     * @param token JWT токен
     * @return userId или nullopt если токен невалиден
     */
    virtual std::optional<std::string> getUserIdFromToken(const std::string& token) = 0;

    /**
     * @brief Извлечь username из токена
     * 
     * @param token JWT токен
     * @return username или nullopt если токен невалиден
     */
    virtual std::optional<std::string> getUsernameFromToken(const std::string& token) = 0;
};

} // namespace trading::ports::input