#pragma once

#include <string>
#include <chrono>

namespace auth::domain {

/**
 * @brief Сессия пользователя
 * 
 * Хранит JWT токен и время его истечения.
 * Позволяет инвалидировать сессию при logout.
 */
struct Session {
    std::string sessionId;      ///< UUID сессии (формат: "sess-xxxxxxxx")
    std::string userId;         ///< Владелец сессии
    std::string jwtToken;       ///< JWT токен
    std::chrono::system_clock::time_point expiresAt;   ///< Время истечения
    std::chrono::system_clock::time_point createdAt;   ///< Время создания

    Session() = default;

    Session(const std::string& sessionId,
            const std::string& userId,
            const std::string& jwtToken,
            std::chrono::seconds lifetime)
        : sessionId(sessionId)
        , userId(userId)
        , jwtToken(jwtToken)
        , createdAt(std::chrono::system_clock::now())
        , expiresAt(createdAt + lifetime)
    {}

    /**
     * @brief Проверить, истекла ли сессия
     */
    bool isExpired() const {
        return std::chrono::system_clock::now() > expiresAt;
    }
};

} // namespace auth::domain
