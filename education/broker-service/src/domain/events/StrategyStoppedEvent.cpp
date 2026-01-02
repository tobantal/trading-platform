#include "domain/events/StrategyStoppedEvent.hpp"
#include <nlohmann/json.hpp>

namespace broker::domain {

std::string StrategyStoppedEvent::toJson() const {
    nlohmann::json j;
    j["eventId"] = eventId;
    j["eventType"] = eventType;
    j["timestamp"] = timestamp.toString();
    j["strategyId"] = strategyId;
    j["accountId"] = accountId;
    j["reason"] = reason;
    return j.dump();
}

} // namespace broker::domain
