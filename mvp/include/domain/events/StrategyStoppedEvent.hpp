// include/domain/events/StrategyStoppedEvent.hpp
#pragma once

#include "DomainEvent.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace trading::domain {

/**
 * @brief Событие: стратегия остановлена
 */
struct StrategyStoppedEvent : public DomainEvent {
    std::string strategyId;
    std::string accountId;
    std::string reason;

    /// Default конструктор
    StrategyStoppedEvent() : DomainEvent("strategy.stopped") {}
    
    /// JSON конструктор для десериализации из RabbitMQ
    explicit StrategyStoppedEvent(const std::string& json) 
        : DomainEvent("strategy.stopped") 
    {
        if (json.empty() || json == "{}") return;
        
        auto j = nlohmann::json::parse(json);
        
        eventId = j.value("eventId", "");
        if (j.contains("timestamp")) {
            timestamp = Timestamp::fromString(j["timestamp"].get<std::string>());
        }
        
        strategyId = j.value("strategyId", "");
        accountId = j.value("accountId", "");
        reason = j.value("reason", "");
    }

    std::string toJson() const override;
    
    std::unique_ptr<DomainEvent> clone() const override {
        return std::make_unique<StrategyStoppedEvent>(*this);
    }
};

} // namespace trading::domain
