// include/domain/events/StrategyStartedEvent.hpp
#pragma once

#include "DomainEvent.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace trading::domain {

/**
 * @brief Событие: стратегия запущена
 */
struct StrategyStartedEvent : public DomainEvent {
    std::string strategyId;
    std::string accountId;

    /// Default конструктор
    StrategyStartedEvent() : DomainEvent("strategy.started") {}
    
    /// JSON конструктор для десериализации из RabbitMQ
    explicit StrategyStartedEvent(const std::string& json) 
        : DomainEvent("strategy.started") 
    {
        if (json.empty() || json == "{}") return;
        
        auto j = nlohmann::json::parse(json);
        
        eventId = j.value("eventId", "");
        if (j.contains("timestamp")) {
            timestamp = Timestamp::fromString(j["timestamp"].get<std::string>());
        }
        
        strategyId = j.value("strategyId", "");
        accountId = j.value("accountId", "");
    }

    std::string toJson() const override;
    
    std::unique_ptr<DomainEvent> clone() const override {
        return std::make_unique<StrategyStartedEvent>(*this);
    }
};

} // namespace trading::domain
