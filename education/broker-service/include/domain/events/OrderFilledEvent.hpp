// include/domain/events/OrderFilledEvent.hpp
#pragma once

#include "DomainEvent.hpp"
#include "domain/Money.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <cstdint>

namespace broker::domain {

/**
 * @brief Событие: ордер исполнен
 */
struct OrderFilledEvent : public DomainEvent {
    std::string orderId;
    std::string accountId;
    std::string figi;
    Money executedPrice;
    int64_t quantity;

    /// Default конструктор
    OrderFilledEvent() : DomainEvent("order.filled") {}
    
    /// JSON конструктор для десериализации из RabbitMQ
    explicit OrderFilledEvent(const std::string& json) 
        : DomainEvent("order.filled") 
    {
        if (json.empty() || json == "{}") return;
        
        auto j = nlohmann::json::parse(json);
        
        eventId = j.value("eventId", "");
        if (j.contains("timestamp")) {
            timestamp = Timestamp::fromString(j["timestamp"].get<std::string>());
        }
        
        orderId = j.value("orderId", "");
        accountId = j.value("accountId", "");
        figi = j.value("figi", "");
        quantity = j.value("quantity", int64_t{0});
        
        if (j.contains("executedPrice")) {
            auto& p = j["executedPrice"];
            executedPrice.units = p.value("units", int64_t{0});
            executedPrice.nano = p.value("nano", 0);
            executedPrice.currency = p.value("currency", "RUB");
        }
    }

    std::string toJson() const override;
    
    std::unique_ptr<DomainEvent> clone() const override {
        return std::make_unique<OrderFilledEvent>(*this);
    }
};

} // namespace broker::domain
