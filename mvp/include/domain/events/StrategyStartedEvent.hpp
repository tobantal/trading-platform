#pragma once

#include "DomainEvent.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Событие: стратегия запущена
 */
struct StrategyStartedEvent : public DomainEvent {
    std::string strategyId;
    std::string accountId;

    StrategyStartedEvent() : DomainEvent("strategy.started") {}

    std::string toJson() const override;
    
    std::unique_ptr<DomainEvent> clone() const override {
        return std::make_unique<StrategyStartedEvent>(*this);
    }
};

} // namespace trading::domain