#pragma once

#include "enums/OrderStatus.hpp"
#include "Money.hpp"
#include "Timestamp.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Результат размещения ордера от брокера
 */
struct OrderResult {
    std::string orderId;        ///< ID ордера от брокера
    OrderStatus status;         ///< Начальный статус
    std::string message;        ///< Сообщение (ошибка или подтверждение)
    Money executedPrice;        ///< Цена исполнения (если FILLED)
    Timestamp timestamp;        ///< Время ответа

    bool isSuccess() const {
        return status != OrderStatus::REJECTED;
    }
};

} // namespace trading::domain