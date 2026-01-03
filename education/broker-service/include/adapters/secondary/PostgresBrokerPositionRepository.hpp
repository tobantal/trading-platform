// include/adapters/secondary/PostgresBrokerPositionRepository.hpp
#pragma once

#include "ports/output/IBrokerPositionRepository.hpp"
#include "settings/DbSettings.hpp"
#include <pqxx/pqxx>
#include <memory>
#include <iostream>

namespace broker::adapters::secondary {

/**
 * @brief PostgreSQL repository for broker positions
 * 
 * BrokerPosition НЕ имеет поля ticker - только accountId, figi, quantity, averagePrice, currency
 */
class PostgresBrokerPositionRepository : public ports::output::IBrokerPositionRepository {
public:
    explicit PostgresBrokerPositionRepository(std::shared_ptr<settings::DbSettings> settings)
        : settings_(std::move(settings))
    {
        // Схема создаётся в init.sql, не здесь
        std::cout << "[PostgresBrokerPositionRepository] Initialized" << std::endl;
    }

    std::vector<domain::BrokerPosition> findByAccountId(const std::string& accountId) override {
        std::vector<domain::BrokerPosition> positions;
        
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            auto result = txn.exec_params(
                "SELECT account_id, figi, quantity, avg_price "
                "FROM broker_positions WHERE account_id = $1",
                accountId
            );
            
            for (const auto& row : result) {
                domain::BrokerPosition pos;
                pos.accountId = row["account_id"].as<std::string>();
                pos.figi = row["figi"].as<std::string>();
                pos.quantity = row["quantity"].as<int64_t>();
                // avg_price в БД в копейках, конвертируем в рубли
                pos.averagePrice = row["avg_price"].as<int64_t>() / 100.0;
                pos.currency = "RUB";
                positions.push_back(pos);
            }
            
            txn.commit();
        } catch (const std::exception& e) {
            std::cerr << "[PostgresBrokerPositionRepository] findByAccountId error: " 
                      << e.what() << std::endl;
        }
        
        return positions;
    }

    std::optional<domain::BrokerPosition> findByAccountAndFigi(
        const std::string& accountId, 
        const std::string& figi) override 
    {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            auto result = txn.exec_params(
                "SELECT account_id, figi, quantity, avg_price "
                "FROM broker_positions WHERE account_id = $1 AND figi = $2",
                accountId, figi
            );
            
            txn.commit();
            
            if (!result.empty()) {
                domain::BrokerPosition pos;
                pos.accountId = result[0]["account_id"].as<std::string>();
                pos.figi = result[0]["figi"].as<std::string>();
                pos.quantity = result[0]["quantity"].as<int64_t>();
                // avg_price в БД в копейках
                pos.averagePrice = result[0]["avg_price"].as<int64_t>() / 100.0;
                pos.currency = "RUB";
                return pos;
            }
        } catch (const std::exception& e) {
            std::cerr << "[PostgresBrokerPositionRepository] findByAccountAndFigi error: " 
                      << e.what() << std::endl;
        }
        
        return std::nullopt;
    }

    void save(const domain::BrokerPosition& position) override {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            // avg_price в БД в копейках
            int64_t avgPriceKopeks = static_cast<int64_t>(position.averagePrice * 100);
            
            txn.exec_params(
                "INSERT INTO broker_positions (account_id, figi, quantity, avg_price, updated_at) "
                "VALUES ($1, $2, $3, $4, NOW()) "
                "ON CONFLICT (account_id, figi) DO UPDATE SET "
                "quantity = EXCLUDED.quantity, "
                "avg_price = EXCLUDED.avg_price, "
                "updated_at = NOW()",
                position.accountId,
                position.figi,
                position.quantity,
                avgPriceKopeks
            );
            
            txn.commit();
        } catch (const std::exception& e) {
            std::cerr << "[PostgresBrokerPositionRepository] save error: " << e.what() << std::endl;
            throw;
        }
    }

    void update(const domain::BrokerPosition& position) override {
        save(position);  // Upsert
    }

private:
    std::shared_ptr<settings::DbSettings> settings_;

    void initSchema() {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            txn.exec(R"(
                CREATE TABLE IF NOT EXISTS broker_positions (
                    account_id VARCHAR(64) NOT NULL,
                    figi VARCHAR(32) NOT NULL,
                    quantity BIGINT NOT NULL DEFAULT 0,
                    average_price DECIMAL(18,8) NOT NULL DEFAULT 0,
                    currency VARCHAR(8) DEFAULT 'RUB',
                    PRIMARY KEY (account_id, figi)
                );
                
                CREATE INDEX IF NOT EXISTS idx_broker_positions_account 
                ON broker_positions(account_id);
            )");
            
            txn.commit();
            std::cout << "[PostgresBrokerPositionRepository] Schema initialized" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[PostgresBrokerPositionRepository] initSchema error: " << e.what() << std::endl;
        }
    }
};

} // namespace broker::adapters::secondary
