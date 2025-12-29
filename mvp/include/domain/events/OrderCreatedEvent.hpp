// include/domain/events/OrderCreatedEvent.hpp
#pragma once

#include "DomainEvent.hpp"
#include "domain/enums/OrderDirection.hpp"
#include "domain/enums/OrderType.hpp"
#include "domain/Money.hpp"
#include <nlohmann/json.hpp>
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

    /// Default конструктор
    OrderCreatedEvent() : DomainEvent("order.created") {}
    
    /// JSON конструктор для десериализации из RabbitMQ
    explicit OrderCreatedEvent(const std::string& json) 
        : DomainEvent("order.created") 
    {
        if (json.empty() || json == "{}") return;
        
        auto j = nlohmann::json::parse(json);
        
        // Базовые поля DomainEvent
        eventId = j.value("eventId", "");
        if (j.contains("timestamp")) {
            timestamp = Timestamp::fromString(j["timestamp"].get<std::string>());
        }
        
        // Поля события
        orderId = j.value("orderId", "");
        accountId = j.value("accountId", "");
        figi = j.value("figi", "");
        quantity = j.value("quantity", int64_t{0});
        
        if (j.contains("direction")) {
            direction = orderDirectionFromString(j["direction"].get<std::string>());
        }
        
        if (j.contains("orderType")) {
            orderType = orderTypeFromString(j["orderType"].get<std::string>());
        }
        
        if (j.contains("price")) {
            auto& p = j["price"];
            price.units = p.value("units", int64_t{0});
            price.nano = p.value("nano", 0);
            price.currency = p.value("currency", "RUB");
        }
    }

    std::string toJson() const override;
    
    std::unique_ptr<DomainEvent> clone() const override {
        return std::make_unique<OrderCreatedEvent>(*this);
    }
};

} // namespace trading::domain
