// include/ports/output/IBrokerOrderRepository.hpp
#pragma once

#include "domain/BrokerOrder.hpp"
#include <vector>
#include <optional>
#include <string>

namespace broker::ports::output {

/**
 * @brief Broker order repository interface
 */
class IBrokerOrderRepository {
public:
    virtual ~IBrokerOrderRepository() = default;

    virtual std::vector<domain::BrokerOrder> findByAccountId(const std::string& accountId) = 0;
    virtual std::optional<domain::BrokerOrder> findById(const std::string& orderId) = 0;
    virtual void save(const domain::BrokerOrder& order) = 0;
    virtual void update(const domain::BrokerOrder& order) = 0;
};

} // namespace broker::ports::output
