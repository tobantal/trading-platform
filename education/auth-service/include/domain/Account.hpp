#pragma once

#include "enums/AccountType.hpp"
#include <string>
#include <chrono>

namespace auth::domain {

/**
 * @brief Брокерский аккаунт пользователя
 * 
 * Пользователь может иметь несколько аккаунтов:
 * - sandbox - для тестирования
 * - real - реальный счёт с Tinkoff API
 */
struct Account {
    std::string accountId;              ///< UUID аккаунта (формат: "acc-xxxxxxxx")
    std::string userId;                 ///< Владелец аккаунта
    std::string name;                   ///< Название ("Мой счёт", "Sandbox #1")
    AccountType type;                   ///< sandbox/real
    std::string tinkoffTokenEncrypted;  ///< Зашифрованный токен Tinkoff API
    std::chrono::system_clock::time_point createdAt;

    Account() = default;

    Account(const std::string& accountId,
            const std::string& userId,
            const std::string& name,
            AccountType type,
            const std::string& tinkoffToken = "")
        : accountId(accountId)
        , userId(userId)
        , name(name)
        , type(type)
        , tinkoffTokenEncrypted(tinkoffToken)
        , createdAt(std::chrono::system_clock::now())
    {}
};

} // namespace auth::domain
