#pragma once

#include "enums/OrderDirection.hpp"
#include "enums/OrderStatus.hpp"
#include "enums/OrderType.hpp"
#include "Money.hpp"
#include "Timestamp.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Торговый ордер (заявка на покупку/продажу)
 */
struct Order {
    std::string id;             ///< UUID ордера
    std::string accountId;      ///< FK на accounts
    std::string figi;           ///< FIGI инструмента
    OrderDirection direction;   ///< BUY / SELL
    OrderType type;             ///< MARKET / LIMIT
    int64_t quantity = 0;       ///< Количество лотов
    Money price;                ///< Цена (для LIMIT ордеров)
    OrderStatus status;         ///< Статус ордера
    Timestamp createdAt;        ///< Дата создания
    Timestamp updatedAt;        ///< Дата последнего обновления

    Order() : status(OrderStatus::PENDING) {}

    Order(
        const std::string& id,
        const std::string& accountId,
        const std::string& figi,
        OrderDirection direction,
        OrderType type,
        int64_t quantity,
        const Money& price = Money()
    ) : id(id), accountId(accountId), figi(figi), direction(direction),
        type(type), quantity(quantity), price(price), status(OrderStatus::PENDING),
        createdAt(Timestamp::now()), updatedAt(Timestamp::now()) {}

    /**
     * @brief Проверить, является ли статус финальным
     */
    bool isFinal() const {
        return isFinalStatus(status);
    }

    /**
     * @brief Обновить статус
     */
    void updateStatus(OrderStatus newStatus) {
        status = newStatus;
        updatedAt = Timestamp::now();
    }

    /**
     * @brief Общая стоимость ордера
     */
    Money totalValue() const {
        return price * quantity;
    }
};

} // namespace trading::domain