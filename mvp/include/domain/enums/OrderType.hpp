#pragma once

#include <string>
#include <stdexcept>

namespace trading::domain {

/**
 * @brief Тип торгового ордера
 */
enum class OrderType {
    MARKET,  ///< Рыночный ордер (исполняется по текущей цене)
    LIMIT    ///< Лимитный ордер (исполняется по указанной цене или лучше)
};

/**
 * @brief Преобразовать в строку
 */
inline std::string toString(OrderType type) {
    switch (type) {
        case OrderType::MARKET: return "MARKET";
        case OrderType::LIMIT:  return "LIMIT";
    }
    return "UNKNOWN";
}

/**
 * @brief Создать из строки
 * @throws std::invalid_argument если строка не распознана
 */
inline OrderType orderTypeFromString(const std::string& str) {
    if (str == "MARKET") return OrderType::MARKET;
    if (str == "LIMIT")  return OrderType::LIMIT;
    throw std::invalid_argument("Unknown OrderType: " + str);
}

/**
 * @brief Требует ли тип ордера указания цены
 */
inline bool requiresPrice(OrderType type) {
    return type == OrderType::LIMIT;
}

} // namespace trading::domain
