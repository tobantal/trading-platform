#pragma once

#include "Money.hpp"
#include "Timestamp.hpp"
#include "enums/OrderDirection.hpp"
#include "enums/OrderType.hpp"
#include "enums/OrderStatus.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Ордер
 */
class Order {
public:
    std::string id;
    std::string accountId;
    std::string figi;
    OrderDirection direction;
    OrderType type;
    int64_t quantity;
    Money price;
    OrderStatus status;
    Timestamp createdAt;
    Timestamp updatedAt;

    Order() 
        : direction(OrderDirection::BUY)
        , type(OrderType::MARKET)
        , quantity(0)
        , status(OrderStatus::PENDING)
    {}

    Order(const std::string& id_, const std::string& accId,
          const std::string& figi_, OrderDirection dir, OrderType t,
          int64_t qty, const Money& p)
        : id(id_)
        , accountId(accId)
        , figi(figi_)
        , direction(dir)
        , type(t)
        , quantity(qty)
        , price(p)
        , status(OrderStatus::PENDING)
        , createdAt(Timestamp::now())
        , updatedAt(Timestamp::now())
    {}

    void updateStatus(OrderStatus newStatus) {
        status = newStatus;
        updatedAt = Timestamp::now();
    }
};

} // namespace trading::domain
