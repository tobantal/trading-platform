#pragma once

#include "DomainEvent.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Событие: ордер отменён
 */
struct OrderCancelledEvent : public DomainEvent {
    std::string orderId;
    std::string accountId;
    std::string reason;

    OrderCancelledEvent() : DomainEvent("order.cancelled") {}

    std::string toJson() const override;
    
    std::unique_ptr<DomainEvent> clone() const override {
        return std::make_unique<OrderCancelledEvent>(*this);
    }
};

} // namespace trading::domain