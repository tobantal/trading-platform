#pragma once

#include "DomainEvent.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Событие: стратегия остановлена
 */
struct StrategyStoppedEvent : public DomainEvent {
    std::string strategyId;
    std::string accountId;
    std::string reason;

    StrategyStoppedEvent() : DomainEvent("strategy.stopped") {}

    std::string toJson() const override;
    
    std::unique_ptr<DomainEvent> clone() const override {
        return std::make_unique<StrategyStoppedEvent>(*this);
    }
};

} // namespace trading::domain