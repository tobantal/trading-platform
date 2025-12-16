#pragma once

#include "DomainEvent.hpp"
#include "domain/Money.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Событие: котировка обновилась
 */
struct QuoteUpdatedEvent : public DomainEvent {
    std::string figi;
    std::string ticker;
    Money lastPrice;
    Money bidPrice;
    Money askPrice;

    QuoteUpdatedEvent() : DomainEvent("quote.updated") {}

    std::string toJson() const override;
    
    std::unique_ptr<DomainEvent> clone() const override {
        return std::make_unique<QuoteUpdatedEvent>(*this);
    }
};

} // namespace trading::domain