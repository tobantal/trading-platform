#pragma once

#include "enums/OrderDirection.hpp"
#include "enums/OrderType.hpp"
#include "Money.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Запрос на создание ордера
 */
struct OrderRequest {
    std::string accountId;      ///< ID счёта
    std::string figi;           ///< FIGI инструмента
    OrderDirection direction;   ///< BUY / SELL
    OrderType type;             ///< MARKET / LIMIT
    int64_t quantity = 0;       ///< Количество лотов
    Money price;                ///< Цена (для LIMIT)
};

} // namespace trading::domain