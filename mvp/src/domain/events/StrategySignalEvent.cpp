#include "domain/events/StrategySignalEvent.hpp"
#include <nlohmann/json.hpp>

namespace trading::domain {

std::string StrategySignalEvent::toJson() const {
    nlohmann::json j;
    j["eventId"] = eventId;
    j["eventType"] = eventType;
    j["timestamp"] = timestamp.toString();
    j["strategyId"] = strategyId;
    j["accountId"] = accountId;
    j["signal"] = toString(signal);
    j["figi"] = figi;
    j["price"] = {
        {"units", price.units},
        {"nano", price.nano},
        {"currency", price.currency}
    };
    j["reason"] = reason;
    j["quantity"] = quantity;
    return j.dump();
}

} // namespace trading::domain