// include/adapters/secondary/PostgresInstrumentRepository.hpp
#pragma once

#include "ports/output/IInstrumentRepository.hpp"
#include "settings/DbSettings.hpp"
#include <pqxx/pqxx>
#include <memory>

namespace broker::adapters::secondary {

/**
 * @brief PostgreSQL instrument repository
 */
class PostgresInstrumentRepository : public ports::output::IInstrumentRepository {
public:
    explicit PostgresInstrumentRepository(std::shared_ptr<settings::DbSettings> settings)
        : settings_(std::move(settings))
    {}

    std::vector<domain::Instrument> findAll() override {
        // TODO: Implement
        return {};
    }

    std::optional<domain::Instrument> findByFigi(const std::string& figi) override {
        // TODO: Implement
        return std::nullopt;
    }

    void save(const domain::Instrument& instrument) override {
        // TODO: Implement
    }

private:
    std::shared_ptr<settings::DbSettings> settings_;
};

} // namespace broker::adapters::secondary
