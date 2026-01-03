#pragma once

#include <string>

namespace trading::domain {

enum class OrderStatus {
    PENDING,
    FILLED,
    PARTIALLY_FILLED,
    CANCELLED,
    REJECTED
};

inline std::string toString(OrderStatus status) {
    switch (status) {
        case OrderStatus::PENDING: return "PENDING";
        case OrderStatus::FILLED: return "FILLED";
        case OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
        case OrderStatus::CANCELLED: return "CANCELLED";
        case OrderStatus::REJECTED: return "REJECTED";
        default: return "UNKNOWN";
    }
}

inline OrderStatus parseOrderStatus(const std::string& str) {
    if (str == "FILLED") return OrderStatus::FILLED;
    if (str == "PARTIALLY_FILLED") return OrderStatus::PARTIALLY_FILLED;
    if (str == "CANCELLED") return OrderStatus::CANCELLED;
    if (str == "REJECTED") return OrderStatus::REJECTED;
    return OrderStatus::PENDING;
}

} // namespace trading::domain
