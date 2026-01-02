#pragma once

#include <string>
#include <stdexcept>

namespace broker::domain {

/**
 * @brief Статус ордера
 */
enum class OrderStatus {
    PENDING,            ///< Ожидает исполнения
    FILLED,             ///< Полностью исполнен
    PARTIALLY_FILLED,   ///< Частично исполнен
    CANCELLED,          ///< Отменён
    REJECTED            ///< Отклонён
};

/**
 * @brief Преобразовать в строку
 */
inline std::string toString(OrderStatus status) {
    switch (status) {
        case OrderStatus::PENDING:          return "PENDING";
        case OrderStatus::FILLED:           return "FILLED";
        case OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
        case OrderStatus::CANCELLED:        return "CANCELLED";
        case OrderStatus::REJECTED:         return "REJECTED";
    }
    return "UNKNOWN";
}

/**
 * @brief Создать из строки
 * @throws std::invalid_argument если строка не распознана
 */
inline OrderStatus orderStatusFromString(const std::string& str) {
    if (str == "PENDING")          return OrderStatus::PENDING;
    if (str == "FILLED")           return OrderStatus::FILLED;
    if (str == "PARTIALLY_FILLED") return OrderStatus::PARTIALLY_FILLED;
    if (str == "CANCELLED")        return OrderStatus::CANCELLED;
    if (str == "REJECTED")         return OrderStatus::REJECTED;
    throw std::invalid_argument("Unknown OrderStatus: " + str);
}

/**
 * @brief Проверить, является ли статус финальным
 */
inline bool isFinalStatus(OrderStatus status) {
    return status == OrderStatus::FILLED ||
           status == OrderStatus::CANCELLED ||
           status == OrderStatus::REJECTED;
}

/**
 * @brief Проверить, является ли статус активным
 */
inline bool isActiveStatus(OrderStatus status) {
    return status == OrderStatus::PENDING ||
           status == OrderStatus::PARTIALLY_FILLED;
}

} // namespace broker::domain
