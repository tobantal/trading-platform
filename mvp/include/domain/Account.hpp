#pragma once

#include "enums/AccountType.hpp"
#include "Timestamp.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Брокерский счёт пользователя
 * 
 * Связывает пользователя системы с брокерским API (Tinkoff).
 * У одного пользователя может быть несколько счетов (sandbox, production).
 */
struct Account {
    std::string id;             ///< UUID счёта
    std::string userId;         ///< FK на users
    std::string name;           ///< Название ("Tinkoff Sandbox", "Основной счёт")
    AccountType type;           ///< SANDBOX / PRODUCTION
    std::string accessToken;    ///< API токен (зашифрован в БД)
    bool active = false;      ///< Активный счёт для текущего пользователя
    Timestamp createdAt;        ///< Дата создания

    Account() = default;

    Account(
        const std::string& id,
        const std::string& userId,
        const std::string& name,
        AccountType type,
        const std::string& accessToken,
        bool isActive = false
    ) : id(id), userId(userId), name(name), type(type), 
        accessToken(accessToken), active(isActive), createdAt(Timestamp::now()) {}
};

} // namespace trading::domain