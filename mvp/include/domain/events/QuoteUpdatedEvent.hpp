// include/domain/events/QuoteUpdatedEvent.hpp
#pragma once

#include "DomainEvent.hpp"
#include "domain/Money.hpp"
#include <nlohmann/json.hpp>
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

    /// Default конструктор
    QuoteUpdatedEvent() : DomainEvent("quote.updated") {}
    
    /// JSON конструктор для десериализации из RabbitMQ
    explicit QuoteUpdatedEvent(const std::string& json) 
        : DomainEvent("quote.updated") 
    {
        if (json.empty() || json == "{}") return;
        
        auto j = nlohmann::json::parse(json);
        
        eventId = j.value("eventId", "");
        if (j.contains("timestamp")) {
            timestamp = Timestamp::fromString(j["timestamp"].get<std::string>());
        }
        
        figi = j.value("figi", "");
        ticker = j.value("ticker", "");
        
        auto parsePrice = [](const nlohmann::json& p) -> Money {
            Money m;
            m.units = p.value("units", int64_t{0});
            m.nano = p.value("nano", 0);
            m.currency = p.value("currency", "RUB");
            return m;
        };
        
        if (j.contains("lastPrice")) lastPrice = parsePrice(j["lastPrice"]);
        if (j.contains("bidPrice")) bidPrice = parsePrice(j["bidPrice"]);
        if (j.contains("askPrice")) askPrice = parsePrice(j["askPrice"]);
    }

    std::string toJson() const override;
    
    std::unique_ptr<DomainEvent> clone() const override {
        return std::make_unique<QuoteUpdatedEvent>(*this);
    }
};

} // namespace trading::domain
