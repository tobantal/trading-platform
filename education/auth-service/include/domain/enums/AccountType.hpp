#pragma once

#include <string>
#include <stdexcept>

namespace auth::domain {

/**
 * @brief Тип брокерского аккаунта
 */
enum class AccountType {
    SANDBOX,    ///< Песочница - виртуальные деньги
    REAL        ///< Реальный счёт
};

/**
 * @brief Преобразовать AccountType в строку
 */
inline std::string toString(AccountType type) {
    switch (type) {
        case AccountType::SANDBOX: return "SANDBOX";
        case AccountType::REAL:    return "REAL";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Преобразовать строку в AccountType
 * @throws std::invalid_argument если строка не распознана
 */
inline AccountType parseAccountType(const std::string& str) {
    if (str == "SANDBOX" || str == "sandbox") return AccountType::SANDBOX;
    if (str == "REAL" || str == "real")       return AccountType::REAL;
    throw std::invalid_argument("Unknown account type: " + str);
}

} // namespace auth::domain
