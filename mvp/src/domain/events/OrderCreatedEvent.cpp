#include "domain/events/OrderCreatedEvent.hpp"
#include <nlohmann/json.hpp>

namespace trading::domain {

std::string OrderCreatedEvent::toJson() const {
    nlohmann::json j;
    j["eventId"] = eventId;
    j["eventType"] = eventType;
    j["timestamp"] = timestamp.toString();
    j["orderId"] = orderId;
    j["accountId"] = accountId;
    j["figi"] = figi;
    j["direction"] = toString(direction);
    j["orderType"] = toString(orderType);
    j["quantity"] = quantity;
    j["price"] = {
        {"units", price.units},
        {"nano", price.nano},
        {"currency", price.currency}
    };
    return j.dump();
}

} // namespace trading::domain
