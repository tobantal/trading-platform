#include "domain/events/QuoteUpdatedEvent.hpp"
#include <nlohmann/json.hpp>

namespace trading::domain {

std::string QuoteUpdatedEvent::toJson() const {
    nlohmann::json j;
    j["eventId"] = eventId;
    j["eventType"] = eventType;
    j["timestamp"] = timestamp.toString();
    j["figi"] = figi;
    j["ticker"] = ticker;
    j["lastPrice"] = {
        {"units", lastPrice.units},
        {"nano", lastPrice.nano},
        {"currency", lastPrice.currency}
    };
    j["bidPrice"] = {
        {"units", bidPrice.units},
        {"nano", bidPrice.nano},
        {"currency", bidPrice.currency}
    };
    j["askPrice"] = {
        {"units", askPrice.units},
        {"nano", askPrice.nano},
        {"currency", askPrice.currency}
    };
    return j.dump();
}

} // namespace trading::domain