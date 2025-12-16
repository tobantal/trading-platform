#pragma once

#include "DomainEvent.hpp"
#include "domain/enums/SignalType.hpp"
#include "domain/Money.hpp"
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

    StrategySignalEvent() : DomainEvent("strategy.signal") {}

    std::string toJson() const override;
    
    std::unique_ptr<DomainEvent> clone() const override {
        return std::make_unique<StrategySignalEvent>(*this);
    }
};

} // namespace trading::domain