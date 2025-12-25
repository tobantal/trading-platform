#include "domain/events/OrderCancelledEvent.hpp"
#include <nlohmann/json.hpp>

namespace trading::domain {

std::string OrderCancelledEvent::toJson() const {
    nlohmann::json j;
    j["eventId"] = eventId;
    j["eventType"] = eventType;
    j["timestamp"] = timestamp.toString();
    j["orderId"] = orderId;
    j["accountId"] = accountId;
    j["reason"] = reason;
    return j.dump();
}

} // namespace trading::domain
