// broker-service/src/domain/events/QuoteUpdatedEvent.cpp
#include "domain/events/QuoteUpdatedEvent.hpp"

namespace broker::domain {

std::string QuoteUpdatedEvent::toJson() const {
    nlohmann::json j;
    j["eventId"] = eventId;
    j["eventType"] = eventType;
    j["timestamp"] = timestamp.toString();
    j["figi"] = figi;
    j["ticker"] = ticker;
    
    // ИСПРАВЛЕНО: отправляем числа и правильные имена полей
    // Было: j["bidPrice"] = {{"units", ...}, {"nano", ...}}
    // Стало: j["bid"] = bidPrice.toDouble()
    j["bid"] = bidPrice.toDouble();
    j["ask"] = askPrice.toDouble();
    j["last_price"] = lastPrice.toDouble();
    j["currency"] = lastPrice.currency;
    
    return j.dump();
}

} // namespace broker::domain