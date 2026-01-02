// include/adapters/secondary/PostgresQuoteRepository.hpp
#pragma once

#include "ports/output/IQuoteRepository.hpp"
#include "settings/DbSettings.hpp"
#include <memory>

namespace broker::adapters::secondary {

/**
 * @brief PostgreSQL quote repository
 */
class PostgresQuoteRepository : public ports::output::IQuoteRepository {
public:
    explicit PostgresQuoteRepository(std::shared_ptr<settings::DbSettings> settings)
        : settings_(std::move(settings))
    {}

    std::optional<domain::Quote> findByFigi(const std::string& figi) override {
        // TODO: Implement
        return std::nullopt;
    }

    void save(const domain::Quote& quote) override {
        // TODO: Implement
    }

private:
    std::shared_ptr<settings::DbSettings> settings_;
};

} // namespace broker::adapters::secondary
