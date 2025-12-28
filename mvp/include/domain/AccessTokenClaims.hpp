#pragma once

#include "domain/Timestamp.hpp"
#include "domain/enums/TokenType.hpp"
#include <string>
#include <random>
#include <sstream>

namespace trading::domain {

/**
 * @brief Claims для Access Token
 * 
 * Access token выдаётся после выбора аккаунта (select-account).
 * Используется для всех торговых операций:
 * - Получение портфеля
 * - Создание/отмена ордеров
 * - Управление стратегиями
 * 
 * КЛЮЧЕВОЕ ОТЛИЧИЕ от Session Token:
 * Содержит accountId — идентификатор брокерского аккаунта.
 * 
 * Время жизни: 1 час (3600 секунд)
 */
struct AccessTokenClaims {
    std::string tokenId;        ///< Уникальный ID токена (jti claim)
    TokenType tokenType;        ///< Всегда TokenType::ACCESS
    Timestamp issuedAt;         ///< Время выдачи (iat claim)
    Timestamp expiresAt;        ///< Время истечения (exp claim)
    std::string accountId;      ///< ID брокерского аккаунта (PRIMARY KEY для торговых операций!)
    std::string userId;         ///< ID пользователя (для аудита)
    std::string username;       ///< Имя пользователя (для логирования)

    /**
     * @brief Конструктор по умолчанию
     * 
     * Создаёт claims с временем жизни 1 час.
     */
    AccessTokenClaims()
        : tokenId(generateTokenId())
        , tokenType(TokenType::ACCESS)
        , issuedAt(Timestamp::now())
        , expiresAt(Timestamp::now().addSeconds(3600))
    {}

    /**
     * @brief Конструктор с параметрами
     * 
     * @param accountId ID брокерского аккаунта (обязательный!)
     * @param userId ID пользователя
     * @param username Имя пользователя
     * @param lifetimeSeconds Время жизни в секундах (по умолчанию 1 час)
     */
    AccessTokenClaims(
        const std::string& accountId,
        const std::string& userId,
        const std::string& username,
        int lifetimeSeconds = 3600
    ) : tokenId(generateTokenId())
      , tokenType(TokenType::ACCESS)
      , issuedAt(Timestamp::now())
      , expiresAt(Timestamp::now().addSeconds(lifetimeSeconds))
      , accountId(accountId)
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
     * @return true если accountId заполнен и токен не истёк
     */
    bool isValid() const {
        return !accountId.empty() && !userId.empty() && !isExpired();
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
