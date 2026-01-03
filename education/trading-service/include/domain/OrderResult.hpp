#pragma once

#include "Money.hpp"
#include "Timestamp.hpp"
#include "enums/OrderStatus.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Результат выполнения ордера
 */
class OrderResult {
public:
    std::string orderId;
    OrderStatus status = OrderStatus::PENDING;
    std::string message;
    Money executedPrice;
    int64_t executedQuantity = 0;
    Timestamp timestamp;

    OrderResult() : timestamp(Timestamp::now()) {}
};

} // namespace trading::domain
