// include/domain/BrokerOrder.hpp
#pragma once

#include <string>

namespace broker::domain {

/**
 * @brief Broker order entity (for persistence)
 */
struct BrokerOrder {
    std::string orderId;
    std::string accountId;
    std::string figi;
    std::string direction;
    std::string orderType;
    std::string status;
    int64_t requestedLots = 0;
    int64_t executedLots = 0;
    double price = 0;
    double executedPrice = 0;
    std::string createdAt;
    std::string updatedAt;
};

} // namespace broker::domain
