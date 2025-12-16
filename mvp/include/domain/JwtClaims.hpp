#pragma once

#include "Timestamp.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Claims из JWT токена
 */
struct JwtClaims { // TODO: rename to JwtUserClaims
    std::string userId;         ///< ID пользователя (sub claim)
    std::string username;       ///< Имя пользователя
    int64_t expiresAt = 0;      ///< Unix timestamp истечения (exp claim)
    int64_t issuedAt = 0;       ///< Unix timestamp выпуска (iat claim)

    /**
     * @brief Проверить, истёк ли токен
     */
    bool isExpired() const {
        auto now = Timestamp::now().toUnixSeconds();
        return now >= expiresAt;
    }
};

} // namespace trading::domain