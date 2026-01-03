#pragma once

#include <string>

namespace trading::domain {

enum class OrderDirection {
    BUY,
    SELL
};

inline std::string toString(OrderDirection dir) {
    switch (dir) {
        case OrderDirection::BUY: return "BUY";
        case OrderDirection::SELL: return "SELL";
        default: return "UNKNOWN";
    }
}

inline OrderDirection parseDirection(const std::string& str) {
    if (str == "SELL") return OrderDirection::SELL;
    return OrderDirection::BUY;
}

} // namespace trading::domain
