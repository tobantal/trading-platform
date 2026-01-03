#pragma once

#include "Money.hpp"
#include "enums/OrderDirection.hpp"
#include "enums/OrderType.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Запрос на создание ордера
 */
class OrderRequest {
public:
    std::string accountId;
    std::string figi;
    OrderDirection direction = OrderDirection::BUY;
    OrderType type = OrderType::MARKET;
    int64_t quantity = 0;
    Money price;

    OrderRequest() = default;
};

} // namespace trading::domain
