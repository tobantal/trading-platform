#pragma once

#include <string>
#include <stdexcept>

namespace trading::domain {

/**
 * @brief Статус торгового ордера
 */
enum class OrderStatus {
    PENDING,    ///< Ожидает исполнения
    FILLED,     ///< Исполнен полностью
    CANCELLED,  ///< Отменён пользователем
    REJECTED    ///< Отклонён брокером
};

/**
 * @brief Преобразовать в строку
 */
inline std::string toString(OrderStatus status) {
    switch (status) {
        case OrderStatus::PENDING:   return "PENDING";
        case OrderStatus::FILLED:    return "FILLED";
        case OrderStatus::CANCELLED: return "CANCELLED";
        case OrderStatus::REJECTED:  return "REJECTED";
    }
    return "UNKNOWN";
}

/**
 * @brief Создать из строки
 * @throws std::invalid_argument если строка не распознана
 */
inline OrderStatus orderStatusFromString(const std::string& str) {
    if (str == "PENDING")   return OrderStatus::PENDING;
    if (str == "FILLED")    return OrderStatus::FILLED;
    if (str == "CANCELLED") return OrderStatus::CANCELLED;
    if (str == "REJECTED")  return OrderStatus::REJECTED;
    throw std::invalid_argument("Unknown OrderStatus: " + str);
}

/**
 * @brief Является ли статус финальным (ордер больше не может измениться)
 */
inline bool isFinalStatus(OrderStatus status) {
    return status == OrderStatus::FILLED || 
           status == OrderStatus::CANCELLED || 
           status == OrderStatus::REJECTED;
}

/**
 * @brief Можно ли отменить ордер в данном статусе
 */
inline bool isCancellable(OrderStatus status) {
    return status == OrderStatus::PENDING;
}

/**
 * @brief Успешно ли завершён ордер
 */
inline bool isSuccessful(OrderStatus status) {
    return status == OrderStatus::FILLED;
}

} // namespace trading::domain
