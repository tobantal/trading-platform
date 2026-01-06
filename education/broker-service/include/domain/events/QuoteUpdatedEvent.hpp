// broker-service/include/domain/events/QuoteUpdatedEvent.hpp
#pragma once

#include "DomainEvent.hpp"
#include "domain/Money.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace broker::domain {

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
    
    /// JSON конструктор для десериализации из RabbitMQ
    /// Поддерживает ОБА формата:
    /// - Новый: {"bid": 100.5, "ask": 101.0, "last_price": 100.75}
    /// - Старый: {"bidPrice": {units, nano}, "askPrice": {units, nano}}
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
        
        std::string currency = j.value("currency", "RUB");
        
        // Поддержка обоих форматов
        // Новый формат: числа "bid", "ask", "last_price"
        if (j.contains("bid") && j["bid"].is_number()) {
            bidPrice = Money::fromDouble(j["bid"].get<double>(), currency);
        }
        if (j.contains("ask") && j["ask"].is_number()) {
            askPrice = Money::fromDouble(j["ask"].get<double>(), currency);
        }
        if (j.contains("last_price") && j["last_price"].is_number()) {
            lastPrice = Money::fromDouble(j["last_price"].get<double>(), currency);
        }
        
        // Старый формат: объекты "bidPrice", "askPrice", "lastPrice"
        auto parsePrice = [](const nlohmann::json& p) -> Money {
            Money m;
            m.units = p.value("units", int64_t{0});
            m.nano = p.value("nano", 0);
            m.currency = p.value("currency", "RUB");
            return m;
        };
        
        if (j.contains("lastPrice") && j["lastPrice"].is_object()) {
            lastPrice = parsePrice(j["lastPrice"]);
        }
        if (j.contains("bidPrice") && j["bidPrice"].is_object()) {
            bidPrice = parsePrice(j["bidPrice"]);
        }
        if (j.contains("askPrice") && j["askPrice"].is_object()) {
            askPrice = parsePrice(j["askPrice"]);
        }
    }

    std::string toJson() const override;
    
    std::unique_ptr<DomainEvent> clone() const override {
        return std::make_unique<QuoteUpdatedEvent>(*this);
    }
};

} // namespace broker::domain