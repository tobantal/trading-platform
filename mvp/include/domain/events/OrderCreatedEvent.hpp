#pragma once

#include "DomainEvent.hpp"
#include "domain/enums/OrderDirection.hpp"
#include "domain/enums/OrderType.hpp"
#include "domain/Money.hpp"
#include <string>
#include <cstdint>

namespace trading::domain {

/**
 * @brief Событие: ордер создан
 */
struct OrderCreatedEvent : public DomainEvent {
    std::string orderId;
    std::string accountId;
    std::string figi;
    OrderDirection direction;
    OrderType orderType;
    int64_t quantity;
    Money price;

    OrderCreatedEvent() : DomainEvent("order.created") {}

    std::string toJson() const override;
    
    std::unique_ptr<DomainEvent> clone() const override {
        return std::make_unique<OrderCreatedEvent>(*this);
    }
};

} // namespace trading::domain