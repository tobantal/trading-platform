// include/ports/output/IBrokerPositionRepository.hpp
#pragma once

#include "domain/BrokerPosition.hpp"
#include <vector>
#include <optional>
#include <string>

namespace broker::ports::output {

/**
 * @brief Broker position repository interface
 */
class IBrokerPositionRepository {
public:
    virtual ~IBrokerPositionRepository() = default;

    virtual std::vector<domain::BrokerPosition> findByAccountId(const std::string& accountId) = 0;
    virtual std::optional<domain::BrokerPosition> findByAccountAndFigi(const std::string& accountId, const std::string& figi) = 0;
    virtual void save(const domain::BrokerPosition& position) = 0;
    virtual void update(const domain::BrokerPosition& position) = 0;
};

} // namespace broker::ports::output
