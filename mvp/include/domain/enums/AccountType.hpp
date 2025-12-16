#pragma once

#include <string>
#include <stdexcept>

namespace trading::domain {

/**
 * @brief Тип брокерского счёта
 */
enum class AccountType {
    SANDBOX,    ///< Песочница (тестовый режим с виртуальными деньгами)
    PRODUCTION  ///< Боевой счёт (реальные деньги)
};

/**
 * @brief Преобразовать в строку
 */
inline std::string toString(AccountType type) {
    switch (type) {
        case AccountType::SANDBOX:    return "SANDBOX";
        case AccountType::PRODUCTION: return "PRODUCTION";
    }
    return "UNKNOWN";
}

/**
 * @brief Создать из строки
 * @throws std::invalid_argument если строка не распознана
 */
inline AccountType accountTypeFromString(const std::string& str) {
    if (str == "SANDBOX")    return AccountType::SANDBOX;
    if (str == "PRODUCTION") return AccountType::PRODUCTION;
    throw std::invalid_argument("Unknown AccountType: " + str);
}

/**
 * @brief Получить человекочитаемое название
 */
inline std::string getDisplayName(AccountType type) {
    switch (type) {
        case AccountType::SANDBOX:    return "Песочница";
        case AccountType::PRODUCTION: return "Боевой счёт";
    }
    return "Неизвестно";
}

/**
 * @brief Является ли счёт тестовым
 */
inline bool isSandbox(AccountType type) {
    return type == AccountType::SANDBOX;
}

/**
 * @brief Является ли счёт боевым
 */
inline bool isProduction(AccountType type) {
    return type == AccountType::PRODUCTION;
}

} // namespace trading::domain