#pragma once

#include "DomainEvent.hpp"
#include "domain/Money.hpp"
#include <string>
#include <cstdint>

namespace trading::domain {

/**
 * @brief Событие: ордер исполнен
 */
struct OrderFilledEvent : public DomainEvent {
    std::string orderId;
    std::string accountId;
    std::string figi;
    Money executedPrice;
    int64_t quantity;

    OrderFilledEvent() : DomainEvent("order.filled") {}

    std::string toJson() const override;
    
    std::unique_ptr<DomainEvent> clone() const override {
        return std::make_unique<OrderFilledEvent>(*this);
    }
};

} // namespace trading::domain