#pragma once

#include "domain/Timestamp.hpp"
#include "domain/enums/TokenType.hpp"
#include <string>
#include <random>
#include <sstream>

namespace trading::domain {

/**
 * @brief Claims для Session Token
 * 
 * Session token выдаётся после успешного login.
 * Используется для:
 * - Получения списка аккаунтов
 * - Выбора аккаунта (select-account)
 * - Обновления токена (refresh)
 * - Выхода из системы (logout)
 * 
 * НЕ содержит accountId — для торговых операций нужен Access Token.
 * 
 * Время жизни: 24 часа (86400 секунд)
 */
struct SessionTokenClaims {
    std::string tokenId;        ///< Уникальный ID токена (jti claim)
    TokenType tokenType;        ///< Всегда TokenType::SESSION
    Timestamp issuedAt;         ///< Время выдачи (iat claim)
    Timestamp expiresAt;        ///< Время истечения (exp claim)
    std::string userId;         ///< ID пользователя (sub claim)
    std::string username;       ///< Имя пользователя (для отображения)

    /**
     * @brief Конструктор по умолчанию
     * 
     * Создаёт claims с временем жизни 24 часа.
     */
    SessionTokenClaims()
        : tokenId(generateTokenId())
        , tokenType(TokenType::SESSION)
        , issuedAt(Timestamp::now())
        , expiresAt(Timestamp::now().addSeconds(86400))
    {}

    /**
     * @brief Конструктор с параметрами
     * 
     * @param userId ID пользователя
     * @param username Имя пользователя
     * @param lifetimeSeconds Время жизни в секундах (по умолчанию 24 часа)
     */
    SessionTokenClaims(
        const std::string& userId,
        const std::string& username,
        int lifetimeSeconds = 86400
    ) : tokenId(generateTokenId())
      , tokenType(TokenType::SESSION)
      , issuedAt(Timestamp::now())
      , expiresAt(Timestamp::now().addSeconds(lifetimeSeconds))
      , userId(userId)
      , username(username)
    {}

    /**
     * @brief Проверить, истёк ли токен
     * 
     * @return true если текущее время > expiresAt
     */
    bool isExpired() const {
        return Timestamp::now() > expiresAt;
    }

    /**
     * @brief Проверить валидность claims
     * 
     * @return true если userId заполнен и токен не истёк
     */
    bool isValid() const {
        return !userId.empty() && !username.empty() && !isExpired();
    }

    /**
     * @brief Получить оставшееся время жизни
     * 
     * @return Секунды до истечения (0 если уже истёк)
     */
    int remainingSeconds() const {
        auto now = Timestamp::now();
        if (now > expiresAt) return 0;
        return static_cast<int>(expiresAt.toUnixSeconds() - now.toUnixSeconds());
    }

private:
    /**
     * @brief Генерация уникального ID токена
     */
    static std::string generateTokenId() {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dist;
        
        std::ostringstream ss;
        ss << std::hex << dist(gen);
        return ss.str();
    }
};

} // namespace trading::domain
