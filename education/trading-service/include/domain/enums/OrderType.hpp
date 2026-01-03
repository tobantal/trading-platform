#pragma once

#include <string>

namespace trading::domain {

enum class OrderType {
    MARKET,
    LIMIT
};

inline std::string toString(OrderType type) {
    switch (type) {
        case OrderType::MARKET: return "MARKET";
        case OrderType::LIMIT: return "LIMIT";
        default: return "UNKNOWN";
    }
}

inline OrderType parseOrderType(const std::string& str) {
    if (str == "LIMIT") return OrderType::LIMIT;
    return OrderType::MARKET;
}

} // namespace trading::domain
