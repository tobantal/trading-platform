// include/adapters/secondary/PostgresBrokerOrderRepository.hpp
#pragma once

#include "ports/output/IBrokerOrderRepository.hpp"
#include "settings/DbSettings.hpp"
#include <memory>

namespace broker::adapters::secondary {

/**
 * @brief PostgreSQL broker order repository
 */
class PostgresBrokerOrderRepository : public ports::output::IBrokerOrderRepository {
public:
    explicit PostgresBrokerOrderRepository(std::shared_ptr<settings::DbSettings> settings)
        : settings_(std::move(settings))
    {}

    std::vector<domain::BrokerOrder> findByAccountId(const std::string& accountId) override {
        return {};
    }

    std::optional<domain::BrokerOrder> findById(const std::string& orderId) override {
        return std::nullopt;
    }

    void save(const domain::BrokerOrder& order) override {}
    void update(const domain::BrokerOrder& order) override {}

private:
    std::shared_ptr<settings::DbSettings> settings_;
};

} // namespace broker::adapters::secondary
