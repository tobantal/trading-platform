#pragma once

#include "ports/output/IJwtProvider.hpp"
#include <nlohmann/json.hpp>
#include <unordered_set>
#include <mutex>
#include <sstream>
#include <iomanip>

namespace trading::adapters::secondary {

/**
 * @brief Fake JWT Provider для разработки и тестирования
 * 
 * Простая реализация без криптографической подписи.
 * Токен имеет формат: header.payload.signature
 *   - header: фиктивный (eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9)
 *   - payload: base64url(JSON)
 *   - signature: "fake_signature"
 * 
 * Поддерживает:
 * - Session Token (24 часа) — после login
 * - Access Token (1 час) — после select-account
 * - Token blacklist — для logout
 * 
 * @warning НЕ использовать в production! Токены не подписаны.
 */
class FakeJwtAdapter : public ports::output::IJwtProvider {
public:
    /**
     * @brief Конструктор
     * 
     * @param sessionLifetimeSeconds Время жизни session token (по умолчанию 24 часа)
     * @param accessLifetimeSeconds Время жизни access token (по умолчанию 1 час)
     */
    explicit FakeJwtAdapter(
        int sessionLifetimeSeconds = 86400,
        int accessLifetimeSeconds = 3600
    ) : sessionLifetime_(sessionLifetimeSeconds)
      , accessLifetime_(accessLifetimeSeconds)
    {}

    // ========================================================================
    // SESSION TOKEN
    // ========================================================================

    /**
     * @brief Создать Session Token
     * 
     * Формирует JWT с claims:
     * - tokenId: уникальный ID
     * - tokenType: "session"
     * - userId, username
     * - iat, exp
     */
    std::string createSessionToken(
        const std::string& userId,
        const std::string& username
    ) override {
        domain::SessionTokenClaims claims(userId, username, sessionLifetime_);
        
        nlohmann::json j;
        j["tokenId"] = claims.tokenId;
        j["tokenType"] = "session";
        j["iat"] = claims.issuedAt.toUnixSeconds();
        j["exp"] = claims.expiresAt.toUnixSeconds();
        j["userId"] = claims.userId;
        j["username"] = claims.username;
        
        std::string payload = base64UrlEncode(j.dump());
        return "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9." + payload + ".fake_signature";
    }

    /**
     * @brief Извлечь claims из Session Token
     * 
     * Проверяет:
     * - Формат токена (header.payload.signature)
     * - tokenType == "session"
     * - Токен не в blacklist
     * - Токен не истёк
     */
    std::optional<domain::SessionTokenClaims> extractSessionClaims(
        const std::string& token
    ) override {
        if (isBlacklisted(token)) {
            return std::nullopt;
        }

        auto jsonOpt = decodePayload(token);
        if (!jsonOpt) {
            return std::nullopt;
        }

        try {
            auto& j = *jsonOpt;
            
            if (j.value("tokenType", "") != "session") {
                return std::nullopt;
            }

            domain::SessionTokenClaims claims;
            claims.tokenId = j.value("tokenId", "");
            claims.tokenType = domain::TokenType::SESSION;
            claims.userId = j.value("userId", "");
            claims.username = j.value("username", "");
            claims.issuedAt = domain::Timestamp::fromUnixSeconds(j.value("iat", int64_t(0)));
            claims.expiresAt = domain::Timestamp::fromUnixSeconds(j.value("exp", int64_t(0)));

            if (claims.isExpired() || claims.userId.empty()) {
                return std::nullopt;
            }

            return claims;
        } catch (...) {
            return std::nullopt;
        }
    }

    // ========================================================================
    // ACCESS TOKEN
    // ========================================================================

    /**
     * @brief Создать Access Token
     * 
     * Формирует JWT с claims:
     * - tokenId: уникальный ID
     * - tokenType: "access"
     * - accountId, userId, username
     * - iat, exp
     */
    std::string createAccessToken(
        const std::string& accountId,
        const std::string& userId,
        const std::string& username
    ) override {
        domain::AccessTokenClaims claims(accountId, userId, username, accessLifetime_);
        
        nlohmann::json j;
        j["tokenId"] = claims.tokenId;
        j["tokenType"] = "access";
        j["iat"] = claims.issuedAt.toUnixSeconds();
        j["exp"] = claims.expiresAt.toUnixSeconds();
        j["accountId"] = claims.accountId;
        j["userId"] = claims.userId;
        j["username"] = claims.username;
        
        std::string payload = base64UrlEncode(j.dump());
        return "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9." + payload + ".fake_signature";
    }

    /**
     * @brief Извлечь claims из Access Token
     * 
     * Проверяет:
     * - Формат токена (header.payload.signature)
     * - tokenType == "access"
     * - Токен не в blacklist
     * - Токен не истёк
     */
    std::optional<domain::AccessTokenClaims> extractAccessClaims(
        const std::string& token
    ) override {
        if (isBlacklisted(token)) {
            return std::nullopt;
        }

        auto jsonOpt = decodePayload(token);
        if (!jsonOpt) {
            return std::nullopt;
        }

        try {
            auto& j = *jsonOpt;
            
            if (j.value("tokenType", "") != "access") {
                return std::nullopt;
            }

            domain::AccessTokenClaims claims;
            claims.tokenId = j.value("tokenId", "");
            claims.tokenType = domain::TokenType::ACCESS;
            claims.accountId = j.value("accountId", "");
            claims.userId = j.value("userId", "");
            claims.username = j.value("username", "");
            claims.issuedAt = domain::Timestamp::fromUnixSeconds(j.value("iat", int64_t(0)));
            claims.expiresAt = domain::Timestamp::fromUnixSeconds(j.value("exp", int64_t(0)));

            if (claims.isExpired() || claims.accountId.empty()) {
                return std::nullopt;
            }

            return claims;
        } catch (...) {
            return std::nullopt;
        }
    }

    // ========================================================================
    // УНИВЕРСАЛЬНЫЕ МЕТОДЫ
    // ========================================================================

    /**
     * @brief Валидировать токен
     * 
     * Проверяет формат, blacklist и срок действия.
     */
    bool validateToken(const std::string& token) override {
        if (isBlacklisted(token)) {
            return false;
        }

        auto jsonOpt = decodePayload(token);
        if (!jsonOpt) {
            return false;
        }

        try {
            auto& j = *jsonOpt;
            int64_t exp = j.value("exp", int64_t(0));
            return domain::Timestamp::now().toUnixSeconds() < exp;
        } catch (...) {
            return false;
        }
    }

    /**
     * @brief Определить тип токена
     */
    std::optional<domain::TokenType> getTokenType(const std::string& token) override {
        auto jsonOpt = decodePayload(token);
        if (!jsonOpt) {
            return std::nullopt;
        }

        try {
            auto& j = *jsonOpt;
            std::string typeStr = j.value("tokenType", "");
            if (typeStr == "session") return domain::TokenType::SESSION;
            if (typeStr == "access")  return domain::TokenType::ACCESS;
            return std::nullopt;
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Извлечь claims из любого токена
     */
    std::optional<ports::output::ExtractedClaims> extractClaims(
        const std::string& token
    ) override {
        auto typeOpt = getTokenType(token);
        if (!typeOpt) {
            return std::nullopt;
        }

        if (*typeOpt == domain::TokenType::SESSION) {
            auto claims = extractSessionClaims(token);
            if (claims) return *claims;
        } else {
            auto claims = extractAccessClaims(token);
            if (claims) return *claims;
        }

        return std::nullopt;
    }

    // ========================================================================
    // ИНВАЛИДАЦИЯ ТОКЕНОВ
    // ========================================================================

    /**
     * @brief Инвалидировать токен
     * 
     * Добавляет токен в blacklist. Thread-safe.
     */
    void invalidateToken(const std::string& token) override {
        std::lock_guard<std::mutex> lock(blacklistMutex_);
        tokenBlacklist_.insert(token);
    }

    /**
     * @brief Инвалидировать все токены пользователя
     * 
     * Добавляет userId в blacklist. Все токены пользователя станут невалидными.
     */
    void invalidateAllUserTokens(const std::string& userId) override {
        std::lock_guard<std::mutex> lock(blacklistMutex_);
        userBlacklist_.insert(userId);
    }

    // ========================================================================
    // КОНФИГУРАЦИЯ
    // ========================================================================

    int getSessionTokenLifetime() const override {
        return sessionLifetime_;
    }

    int getAccessTokenLifetime() const override {
        return accessLifetime_;
    }

    // ========================================================================
    // TEST HELPERS
    // ========================================================================

    /**
     * @brief Очистить blacklist (для тестов)
     */
    void clearBlacklist() {
        std::lock_guard<std::mutex> lock(blacklistMutex_);
        tokenBlacklist_.clear();
        userBlacklist_.clear();
    }

private:
    int sessionLifetime_;
    int accessLifetime_;
    
    std::unordered_set<std::string> tokenBlacklist_;
    std::unordered_set<std::string> userBlacklist_;
    mutable std::mutex blacklistMutex_;

    /**
     * @brief Декодировать payload из JWT
     * 
     * @param token JWT в формате header.payload.signature
     * @return JSON или nullopt если формат неверный
     */
    std::optional<nlohmann::json> decodePayload(const std::string& token) {
        try {
            size_t firstDot = token.find('.');
            size_t lastDot = token.rfind('.');
            
            if (firstDot == std::string::npos || 
                lastDot == std::string::npos || 
                firstDot == lastDot) {
                return std::nullopt;
            }

            std::string payload = token.substr(firstDot + 1, lastDot - firstDot - 1);
            std::string decoded = base64UrlDecode(payload);
            return nlohmann::json::parse(decoded);
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Проверить, находится ли токен в blacklist
     */
    bool isBlacklisted(const std::string& token) {
        std::lock_guard<std::mutex> lock(blacklistMutex_);
        
        // Проверяем сам токен
        if (tokenBlacklist_.count(token) > 0) {
            return true;
        }

        // Проверяем userId
        auto jsonOpt = decodePayload(token);
        if (jsonOpt) {
            std::string userId = (*jsonOpt).value("userId", "");
            if (!userId.empty() && userBlacklist_.count(userId) > 0) {
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Base64 URL-safe кодирование
     */
    static std::string base64UrlEncode(const std::string& input) {
        static const char* chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        std::string result;
        
        int val = 0, valb = -6;
        for (unsigned char c : input) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                result.push_back(chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) {
            result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
        }
        return result;
    }

    /**
     * @brief Base64 URL-safe декодирование
     */
    static std::string base64UrlDecode(const std::string& input) {
        static const int lookup[] = {
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,
            52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
            -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
            15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63,
            -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
            41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
        };
        
        std::string result;
        int val = 0, valb = -8;
        
        for (unsigned char c : input) {
            if (c >= 128 || lookup[c] == -1) continue;
            val = (val << 6) + lookup[c];
            valb += 6;
            if (valb >= 0) {
                result.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return result;
    }
};

} // namespace trading::adapters::secondary
