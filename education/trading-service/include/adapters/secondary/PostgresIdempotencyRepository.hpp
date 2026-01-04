#pragma once

#include "ports/output/IIdempotencyRepository.hpp"
#include "settings/DbSettings.hpp"
#include <pqxx/pqxx>
#include <memory>
#include <iostream>

namespace trading::adapters::secondary
{

    class PostgresIdempotencyRepository : public trading::ports::output::IIdempotencyRepository
    {
    public:
        explicit PostgresIdempotencyRepository(std::shared_ptr<trading::settings::DbSettings> s) : settings_(std::move(s))
        {
            // Проверяем соединение, но не создаём таблицу
            pqxx::connection c(settings_->getConnectionString());
            std::cout << "[IdempotencyRepo] Connected to " << settings_->getName() << std::endl;
        }

        std::optional<trading::domain::IdempotencyRecord> find(const std::string &key) override
        {
            pqxx::connection c(settings_->getConnectionString());
            pqxx::work t(c);
            auto r = t.exec_params(
                "SELECT key, response_status, response_body FROM idempotency_keys WHERE key=$1", key);
            if (r.empty())
                return std::nullopt;
            return trading::domain::IdempotencyRecord{
                r[0][0].as<std::string>(),
                r[0][1].as<int>(),
                r[0][2].as<std::string>()};
        }

        void save(const std::string &key, int status, const std::string &body) override
        {
            pqxx::connection c(settings_->getConnectionString());
            pqxx::work t(c);
            t.exec_params(
                "INSERT INTO idempotency_keys (key, response_status, response_body) VALUES ($1, $2, $3) "
                "ON CONFLICT (key) DO NOTHING",
                key, status, body);
            t.commit();
            std::cout << "[IdempotencyRepo] Saved key: " << key << std::endl;
        }

    private:
        std::shared_ptr<trading::settings::DbSettings> settings_;
    };

} // namespace trading::adapters::secondary
