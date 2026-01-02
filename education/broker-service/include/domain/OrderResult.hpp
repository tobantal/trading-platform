#pragma once

#include "enums/OrderStatus.hpp"
#include "Money.hpp"
#include "Timestamp.hpp"
#include <string>

namespace broker::domain {

/**
 * @brief Результат размещения ордера от брокера
 */
struct OrderResult {
    std::string orderId;        ///< ID ордера от брокера
    OrderStatus status;         ///< Начальный статус
    std::string message;        ///< Сообщение (ошибка или подтверждение)
    Money executedPrice;        ///< Цена исполнения (если FILLED)
    int64_t executedLots = 0;   ///< Количество исполненных лотов
    Timestamp timestamp;        ///< Время ответа

    bool isSuccess() const {
        return status != OrderStatus::REJECTED;
    }
};

} // namespace broker::domain
