#pragma once

#include <string>
#include <stdexcept>

namespace trading::domain {

/**
 * @brief Тип JWT токена
 * 
 * Система использует двухуровневую авторизацию:
 * - SESSION — для управления сессией (24 часа)
 * - ACCESS — для торговых операций (1 час)
 */
enum class TokenType {
    SESSION,    ///< Session token — управление сессией, выбор аккаунта
    ACCESS      ///< Access token — торговые операции (содержит accountId)
};

/**
 * @brief Преобразовать TokenType в строку
 * 
 * @param type Тип токена
 * @return "session" или "access"
 */
inline std::string toString(TokenType type) {
    switch (type) {
        case TokenType::SESSION: return "session";
        case TokenType::ACCESS:  return "access";
    }
    return "unknown";
}

/**
 * @brief Создать TokenType из строки
 * 
 * @param str Строка ("session" или "access")
 * @return TokenType
 * @throws std::invalid_argument если строка не распознана
 */
inline TokenType tokenTypeFromString(const std::string& str) {
    if (str == "session") return TokenType::SESSION;
    if (str == "access")  return TokenType::ACCESS;
    throw std::invalid_argument("Unknown TokenType: " + str);
}

} // namespace trading::domain
