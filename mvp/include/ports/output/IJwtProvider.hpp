#pragma once

#include "domain/JwtClaims.hpp"
#include "domain/User.hpp"
#include <string>
#include <optional>

namespace trading::ports::output {

/**
 * @brief Интерфейс провайдера JWT токенов
 * 
 * Output Port для создания и валидации JWT токенов.
 * 
 * Реализации:
 * - FakeJwtAdapter (MVP) - работает с fake-jwt-server
 * - ZitadelAdapter (Education) - работает с ZITADEL
 */
class IJwtProvider {
public:
    virtual ~IJwtProvider() = default;

    /**
     * @brief Создать JWT токен для пользователя
     * 
     * @param userId UUID пользователя
     * @param username Имя пользователя
     * @return JWT токен
     */
    virtual std::string createToken(
        const std::string& userId,
        const std::string& username
    ) = 0;

    /**
     * @brief Валидировать JWT токен
     * 
     * @param token JWT токен
     * @return true если токен валиден и не истёк
     */
    virtual bool validateToken(const std::string& token) = 0;

    /**
     * @brief Извлечь claims из токена
     * 
     * @param token JWT токен
     * @return JwtClaims или nullopt если токен невалиден
     */
    virtual std::optional<domain::JwtClaims> extractClaims(const std::string& token) = 0;

    /**
     * @brief Обновить токен (продлить время жизни)
     * 
     * @param token Текущий токен
     * @return Новый токен или пустая строка если обновление невозможно
     */
    virtual std::string refreshToken(const std::string& token) = 0;
};

} // namespace trading::ports::output