// include/adapters/secondary/PostgresBrokerBalanceRepository.hpp
#pragma once

#include "ports/output/IBrokerBalanceRepository.hpp"
#include "settings/DbSettings.hpp"
#include <pqxx/pqxx>
#include <memory>
#include <iostream>

namespace broker::adapters::secondary {

/**
 * @brief PostgreSQL реализация репозитория баланса
 * 
 * Таблица: broker_balances
 * - account_id VARCHAR(64) PRIMARY KEY
 * - currency VARCHAR(3) NOT NULL DEFAULT 'RUB'
 * - available BIGINT NOT NULL DEFAULT 0  (в копейках)
 * - reserved BIGINT NOT NULL DEFAULT 0   (в копейках)
 * - updated_at TIMESTAMP DEFAULT NOW()
 */
class PostgresBrokerBalanceRepository : public ports::output::IBrokerBalanceRepository {
public:
    explicit PostgresBrokerBalanceRepository(std::shared_ptr<settings::DbSettings> settings)
        : settings_(std::move(settings))
    {
        initSchema();
    }

    std::optional<domain::BrokerBalance> findByAccountId(const std::string& accountId) override {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            auto result = txn.exec_params(
                "SELECT account_id, currency, available, reserved "
                "FROM broker_balances WHERE account_id = $1",
                accountId
            );
            
            if (result.empty()) {
                return std::nullopt;
            }
            
            domain::BrokerBalance balance;
            balance.accountId = result[0]["account_id"].as<std::string>();
            balance.currency = result[0]["currency"].as<std::string>();
            balance.available = result[0]["available"].as<int64_t>();
            balance.reserved = result[0]["reserved"].as<int64_t>();
            
            return balance;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresBrokerBalanceRepository] findByAccountId error: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    void save(const domain::BrokerBalance& balance) override {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            txn.exec_params(
                "INSERT INTO broker_balances (account_id, currency, available, reserved, updated_at) "
                "VALUES ($1, $2, $3, $4, NOW()) "
                "ON CONFLICT (account_id) DO UPDATE SET "
                "currency = EXCLUDED.currency, "
                "available = EXCLUDED.available, "
                "reserved = EXCLUDED.reserved, "
                "updated_at = NOW()",
                balance.accountId,
                balance.currency,
                balance.available,
                balance.reserved
            );
            
            txn.commit();
            std::cout << "[PostgresBrokerBalanceRepository] Saved balance for " << balance.accountId << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresBrokerBalanceRepository] save error: " << e.what() << std::endl;
            throw;
        }
    }

    void update(const domain::BrokerBalance& balance) override {
        save(balance);  // UPSERT handles both
    }
    
    bool reserve(const std::string& accountId, int64_t amount) override {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            // Атомарное резервирование с проверкой
            auto result = txn.exec_params(
                "UPDATE broker_balances "
                "SET available = available - $2, "
                "    reserved = reserved + $2, "
                "    updated_at = NOW() "
                "WHERE account_id = $1 AND available >= $2 "
                "RETURNING account_id",
                accountId,
                amount
            );
            
            if (result.empty()) {
                // Недостаточно средств или аккаунт не найден
                return false;
            }
            
            txn.commit();
            std::cout << "[PostgresBrokerBalanceRepository] Reserved " << amount << " for " << accountId << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresBrokerBalanceRepository] reserve error: " << e.what() << std::endl;
            return false;
        }
    }
    
    void commitReserved(const std::string& accountId, int64_t amount) override {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            txn.exec_params(
                "UPDATE broker_balances "
                "SET reserved = reserved - $2, updated_at = NOW() "
                "WHERE account_id = $1",
                accountId,
                amount
            );
            
            txn.commit();
            std::cout << "[PostgresBrokerBalanceRepository] Committed " << amount << " for " << accountId << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresBrokerBalanceRepository] commitReserved error: " << e.what() << std::endl;
        }
    }
    
    void releaseReserved(const std::string& accountId, int64_t amount) override {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            txn.exec_params(
                "UPDATE broker_balances "
                "SET available = available + $2, "
                "    reserved = reserved - $2, "
                "    updated_at = NOW() "
                "WHERE account_id = $1",
                accountId,
                amount
            );
            
            txn.commit();
            std::cout << "[PostgresBrokerBalanceRepository] Released " << amount << " for " << accountId << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresBrokerBalanceRepository] releaseReserved error: " << e.what() << std::endl;
        }
    }

private:
    std::shared_ptr<settings::DbSettings> settings_;
    
    void initSchema() {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            txn.exec(R"(
                CREATE TABLE IF NOT EXISTS broker_balances (
                    account_id VARCHAR(64) PRIMARY KEY,
                    currency VARCHAR(3) NOT NULL DEFAULT 'RUB',
                    available BIGINT NOT NULL DEFAULT 0,
                    reserved BIGINT NOT NULL DEFAULT 0,
                    updated_at TIMESTAMP DEFAULT NOW()
                )
            )");
            
            txn.commit();
            std::cout << "[PostgresBrokerBalanceRepository] Schema initialized" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresBrokerBalanceRepository] initSchema error: " << e.what() << std::endl;
        }
    }
};

} // namespace broker::adapters::secondary
