// include/adapters/secondary/PostgresBrokerPositionRepository.hpp
#pragma once

#include "ports/output/IBrokerPositionRepository.hpp"
#include "settings/DbSettings.hpp"
#include <memory>

namespace broker::adapters::secondary {

/**
 * @brief PostgreSQL broker position repository
 */
class PostgresBrokerPositionRepository : public ports::output::IBrokerPositionRepository {
public:
    explicit PostgresBrokerPositionRepository(std::shared_ptr<settings::DbSettings> settings)
        : settings_(std::move(settings))
    {}

    std::vector<domain::BrokerPosition> findByAccountId(const std::string& accountId) override {
        return {};
    }

    std::optional<domain::BrokerPosition> findByAccountAndFigi(const std::string& accountId, const std::string& figi) override {
        return std::nullopt;
    }

    void save(const domain::BrokerPosition& position) override {}
    void update(const domain::BrokerPosition& position) override {}

private:
    std::shared_ptr<settings::DbSettings> settings_;
};

} // namespace broker::adapters::secondary
