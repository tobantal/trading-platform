#pragma once

#include "ports/output/IJwtProvider.hpp"
#include <unordered_map>
#include <mutex>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace trading::adapters::secondary {

/**
 * @brief Встроенный fake JWT провайдер
 * 
 * Генерирует простые JWT-подобные токены для MVP.
 * Формат: base64(header).base64(payload).signature
 * 
 * НЕ использует криптографическую подпись - только для разработки!
 * В Education заменяется на ZitadelAdapter.
 */
class FakeJwtAdapter : public ports::output::IJwtProvider {
public:
    FakeJwtAdapter(int tokenLifetimeSeconds = 3600) 
        : tokenLifetimeSeconds_(tokenLifetimeSeconds)
        , rng_(std::random_device{}()) 
    {}

    /**
     * @brief Создать JWT токен для пользователя
     */
    std::string createToken(
        const std::string& userId,
        const std::string& username
    ) override {
        auto now = std::chrono::system_clock::now();
        auto nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        auto expSeconds = nowSeconds + tokenLifetimeSeconds_;

        // Создаём payload
        std::stringstream payload;
        payload << R"({"sub":")" << userId << R"(",)"
                << R"("username":")" << username << R"(",)"
                << R"("iat":)" << nowSeconds << ","
                << R"("exp":)" << expSeconds << "}";

        // Простой header
        std::string header = R"({"alg":"HS256","typ":"JWT"})";

        // Генерируем токен
        std::string token = base64Encode(header) + "." + 
                           base64Encode(payload.str()) + "." +
                           generateSignature();

        // Сохраняем для валидации
        std::lock_guard<std::mutex> lock(tokensMutex_);
        TokenInfo info;
        info.userId = userId;
        info.username = username;
        info.issuedAt = nowSeconds;
        info.expiresAt = expSeconds;
        tokens_[token] = info;

        return token;
    }

    /**
     * @brief Валидировать JWT токен
     */
    bool validateToken(const std::string& token) override {
        std::lock_guard<std::mutex> lock(tokensMutex_);
        
        auto it = tokens_.find(token);
        if (it == tokens_.end()) {
            return false;
        }

        auto now = std::chrono::system_clock::now();
        auto nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();

        return nowSeconds < it->second.expiresAt;
    }

    /**
     * @brief Извлечь claims из токена
     */
    std::optional<domain::JwtClaims> extractClaims(const std::string& token) override {
        std::lock_guard<std::mutex> lock(tokensMutex_);
        
        auto it = tokens_.find(token);
        if (it == tokens_.end()) {
            return std::nullopt;
        }

        domain::JwtClaims claims;
        claims.userId = it->second.userId;
        claims.username = it->second.username;
        claims.issuedAt = it->second.issuedAt;
        claims.expiresAt = it->second.expiresAt;

        return claims;
    }

    /**
     * @brief Обновить токен (продлить время жизни)
     */
    std::string refreshToken(const std::string& token) override {
        auto claims = extractClaims(token);
        if (!claims) {
            return "";
        }

        // Удаляем старый токен
        {
            std::lock_guard<std::mutex> lock(tokensMutex_);
            tokens_.erase(token);
        }

        // Создаём новый
        return createToken(claims->userId, claims->username);
    }

    /**
     * @brief Отозвать токен (для logout)
     */
    void revokeToken(const std::string& token) {
        std::lock_guard<std::mutex> lock(tokensMutex_);
        tokens_.erase(token);
    }

    /**
     * @brief Очистить все токены (для тестов)
     */
    void clear() {
        std::lock_guard<std::mutex> lock(tokensMutex_);
        tokens_.clear();
    }

    /**
     * @brief Получить количество активных токенов
     */
    size_t activeTokenCount() const {
        std::lock_guard<std::mutex> lock(tokensMutex_);
        return tokens_.size();
    }

private:
    struct TokenInfo {
        std::string userId;
        std::string username;
        int64_t issuedAt;
        int64_t expiresAt;
    };

    int tokenLifetimeSeconds_;
    mutable std::mutex tokensMutex_;
    std::unordered_map<std::string, TokenInfo> tokens_;
    std::mt19937_64 rng_;

    /**
     * @brief Простое base64 кодирование (URL-safe)
     */
    std::string base64Encode(const std::string& input) {
        static const char* chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        
        std::string result;
        result.reserve(((input.size() + 2) / 3) * 4);

        for (size_t i = 0; i < input.size(); i += 3) {
            uint32_t n = static_cast<uint8_t>(input[i]) << 16;
            if (i + 1 < input.size()) n |= static_cast<uint8_t>(input[i + 1]) << 8;
            if (i + 2 < input.size()) n |= static_cast<uint8_t>(input[i + 2]);

            result += chars[(n >> 18) & 0x3F];
            result += chars[(n >> 12) & 0x3F];
            result += (i + 1 < input.size()) ? chars[(n >> 6) & 0x3F] : '=';
            result += (i + 2 < input.size()) ? chars[n & 0x3F] : '=';
        }

        // Убираем padding для URL-safe
        while (!result.empty() && result.back() == '=') {
            result.pop_back();
        }

        return result;
    }

    /**
     * @brief Генерация псевдо-подписи (НЕ криптографическая!)
     */
    std::string generateSignature() {
        std::uniform_int_distribution<uint64_t> dist;
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        ss << std::setw(16) << dist(rng_);
        ss << std::setw(16) << dist(rng_);
        return ss.str();
    }
};

} // namespace trading::adapters::secondary
