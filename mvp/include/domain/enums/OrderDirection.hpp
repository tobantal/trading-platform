#pragma once

#include <string>
#include <stdexcept>

namespace trading::domain {

/**
 * @brief Направление торгового ордера
 */
enum class OrderDirection {
    BUY,    ///< Покупка
    SELL    ///< Продажа
};

/**
 * @brief Преобразовать в строку
 */
inline std::string toString(OrderDirection dir) {
    switch (dir) {
        case OrderDirection::BUY:  return "BUY";
        case OrderDirection::SELL: return "SELL";
    }
    return "UNKNOWN";
}

/**
 * @brief Создать из строки
 * @throws std::invalid_argument если строка не распознана
 */
inline OrderDirection orderDirectionFromString(const std::string& str) {
    if (str == "BUY")  return OrderDirection::BUY;
    if (str == "SELL") return OrderDirection::SELL;
    throw std::invalid_argument("Unknown OrderDirection: " + str);
}

/**
 * @brief Получить противоположное направление
 */
inline OrderDirection opposite(OrderDirection dir) {
    return dir == OrderDirection::BUY ? OrderDirection::SELL : OrderDirection::BUY;
}

} // namespace trading::domain
