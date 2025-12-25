#include "domain/events/StrategyStartedEvent.hpp"
#include <nlohmann/json.hpp>

namespace trading::domain {

std::string StrategyStartedEvent::toJson() const {
    nlohmann::json j;
    j["eventId"] = eventId;
    j["eventType"] = eventType;
    j["timestamp"] = timestamp.toString();
    j["strategyId"] = strategyId;
    j["accountId"] = accountId;
    return j.dump();
}

} // namespace trading::domain
