#include "domain/events/OrderFilledEvent.hpp"
#include <nlohmann/json.hpp>

namespace broker::domain {

std::string OrderFilledEvent::toJson() const {
    nlohmann::json j;
    j["eventId"] = eventId;
    j["eventType"] = eventType;
    j["timestamp"] = timestamp.toString();
    j["orderId"] = orderId;
    j["accountId"] = accountId;
    j["figi"] = figi;
    j["executedPrice"] = {
        {"units", executedPrice.units},
        {"nano", executedPrice.nano},
        {"currency", executedPrice.currency}
    };
    j["quantity"] = quantity;
    return j.dump();
}

} // namespace broker::domain
