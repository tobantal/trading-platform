#pragma once

#include "domain/SessionTokenClaims.hpp"
#include "domain/AccessTokenClaims.hpp"
#include "domain/enums/TokenType.hpp"
#include <string>
#include <optional>
#include <variant>

namespace trading::ports::output {

/**
 * @brief Результат извлечения claims из токена
 * 
 * Может содержать SessionTokenClaims или AccessTokenClaims.
 */
using ExtractedClaims = std::variant<domain::SessionTokenClaims, domain::AccessTokenClaims>;

/**
 * @brief Интерфейс провайдера JWT токенов
 * 
 * Output Port для создания и валидации JWT токенов.
 * Поддерживает двухуровневую авторизацию:
 * - Session Token (после login) — для управления сессией
 * - Access Token (после select-account) — для торговых операций
 * 
 * Реализации:
 * - FakeJwtAdapter — простая эмуляция без криптографии (для разработки/тестов)
 * - JwtAdapter — реальная реализация с подписью RS256/HS256 (для production)
 */
class IJwtProvider {
public:
    virtual ~IJwtProvider() = default;

    // ========================================================================
    // SESSION TOKEN
    // ========================================================================

    /**
     * @brief Создать Session Token
     * 
     * @param userId ID пользователя (будет в sub claim)
     * @param username Username (для отображения)
     * @return JWT строка session token
     * 
     * @note Время жизни: 24 часа (86400 секунд)
     */
    virtual std::string createSessionToken(
        const std::string& userId,
        const std::string& username
    ) = 0;

    /**
     * @brief Извлечь claims из Session Token
     * 
     * @param token JWT строка
     * @return SessionTokenClaims или nullopt если:
     *   - токен невалиден (неверный формат, подпись)
     *   - токен истёк
     *   - токен в blacklist
     *   - это не session token (а access)
     */
    virtual std::optional<domain::SessionTokenClaims> extractSessionClaims(
        const std::string& token
    ) = 0;

    // ========================================================================
    // ACCESS TOKEN
    // ========================================================================

    /**
     * @brief Создать Access Token
     * 
     * @param accountId ID брокерского аккаунта (PRIMARY KEY для торговых операций!)
     * @param userId ID пользователя (для аудита)
     * @param username Username (для логирования)
     * @return JWT строка access token
     * 
     * @note Время жизни: 1 час (3600 секунд)
     */
    virtual std::string createAccessToken(
        const std::string& accountId,
        const std::string& userId,
        const std::string& username
    ) = 0;

    /**
     * @brief Извлечь claims из Access Token
     * 
     * @param token JWT строка
     * @return AccessTokenClaims или nullopt если:
     *   - токен невалиден (неверный формат, подпись)
     *   - токен истёк
     *   - токен в blacklist
     *   - это не access token (а session)
     */
    virtual std::optional<domain::AccessTokenClaims> extractAccessClaims(
        const std::string& token
    ) = 0;

    // ========================================================================
    // УНИВЕРСАЛЬНЫЕ МЕТОДЫ
    // ========================================================================

    /**
     * @brief Валидировать любой токен (session или access)
     * 
     * @param token JWT строка
     * @return true если токен валиден и не истёк
     */
    virtual bool validateToken(const std::string& token) = 0;

    /**
     * @brief Определить тип токена
     * 
     * @param token JWT строка
     * @return TokenType или nullopt если токен невалиден
     */
    virtual std::optional<domain::TokenType> getTokenType(const std::string& token) = 0;

    /**
     * @brief Извлечь claims из любого токена
     * 
     * @param token JWT строка
     * @return variant с SessionTokenClaims или AccessTokenClaims, или nullopt
     */
    virtual std::optional<ExtractedClaims> extractClaims(const std::string& token) = 0;

    // ========================================================================
    // ИНВАЛИДАЦИЯ ТОКЕНОВ
    // ========================================================================

    /**
     * @brief Инвалидировать токен
     * 
     * Добавляет токен в blacklist. После этого токен считается невалидным
     * даже если срок его действия ещё не истёк.
     * 
     * @param token JWT строка
     */
    virtual void invalidateToken(const std::string& token) = 0;

    /**
     * @brief Инвалидировать все токены пользователя
     * 
     * Добавляет userId в blacklist. Все токены этого пользователя
     * становятся невалидными.
     * 
     * @param userId ID пользователя
     */
    virtual void invalidateAllUserTokens(const std::string& userId) = 0;

    // ========================================================================
    // КОНФИГУРАЦИЯ
    // ========================================================================

    /**
     * @brief Получить время жизни Session Token в секундах
     * 
     * @return Время жизни (по умолчанию 86400 = 24 часа)
     */
    virtual int getSessionTokenLifetime() const = 0;

    /**
     * @brief Получить время жизни Access Token в секундах
     * 
     * @return Время жизни (по умолчанию 3600 = 1 час)
     */
    virtual int getAccessTokenLifetime() const = 0;
};

} // namespace trading::ports::output
