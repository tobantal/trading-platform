// include/domain/BrokerPosition.hpp
#pragma once

#include <string>

namespace broker::domain {

/**
 * @brief Broker position entity (for persistence)
 */
struct BrokerPosition {
    std::string accountId;
    std::string figi;
    int64_t quantity = 0;
    double averagePrice = 0;
    std::string currency;
};

} // namespace broker::domain
