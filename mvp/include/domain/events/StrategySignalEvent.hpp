// include/domain/events/StrategySignalEvent.hpp
#pragma once

#include "DomainEvent.hpp"
#include "domain/enums/SignalType.hpp"
#include "domain/Money.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <cstdint>

namespace trading::domain {

/**
 * @brief Событие: стратегия сгенерировала сигнал
 */
struct StrategySignalEvent : public DomainEvent {
    std::string strategyId;
    std::string accountId;
    SignalType signal;
    std::string figi;
    Money price;
    std::string reason;
    int64_t quantity;  ///< Рекомендуемое количество лотов

    /// Default конструктор
    StrategySignalEvent() : DomainEvent("strategy.signal") {}
    
    /// JSON конструктор для десериализации из RabbitMQ
    explicit StrategySignalEvent(const std::string& json) 
        : DomainEvent("strategy.signal") 
    {
        if (json.empty() || json == "{}") return;
        
        auto j = nlohmann::json::parse(json);
        
        eventId = j.value("eventId", "");
        if (j.contains("timestamp")) {
            timestamp = Timestamp::fromString(j["timestamp"].get<std::string>());
        }
        
        strategyId = j.value("strategyId", "");
        accountId = j.value("accountId", "");
        figi = j.value("figi", "");
        reason = j.value("reason", "");
        quantity = j.value("quantity", int64_t{0});
        
        if (j.contains("signal")) {
            signal = signalTypeFromString(j["signal"].get<std::string>());
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
        return std::make_unique<StrategySignalEvent>(*this);
    }
};

} // namespace trading::domain
