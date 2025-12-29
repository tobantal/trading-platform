// include/domain/events/OrderCancelledEvent.hpp
#pragma once

#include "DomainEvent.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace trading::domain {

/**
 * @brief Событие: ордер отменён
 */
struct OrderCancelledEvent : public DomainEvent {
    std::string orderId;
    std::string accountId;
    std::string reason;

    /// Default конструктор
    OrderCancelledEvent() : DomainEvent("order.cancelled") {}
    
    /// JSON конструктор для десериализации из RabbitMQ
    explicit OrderCancelledEvent(const std::string& json) 
        : DomainEvent("order.cancelled") 
    {
        if (json.empty() || json == "{}") return;
        
        auto j = nlohmann::json::parse(json);
        
        eventId = j.value("eventId", "");
        if (j.contains("timestamp")) {
            timestamp = Timestamp::fromString(j["timestamp"].get<std::string>());
        }
        
        orderId = j.value("orderId", "");
        accountId = j.value("accountId", "");
        reason = j.value("reason", "");
    }

    std::string toJson() const override;
    
    std::unique_ptr<DomainEvent> clone() const override {
        return std::make_unique<OrderCancelledEvent>(*this);
    }
};

} // namespace trading::domain
