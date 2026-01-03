// include/adapters/secondary/PostgresBrokerOrderRepository.hpp
#pragma once

#include "ports/output/IBrokerOrderRepository.hpp"
#include "settings/DbSettings.hpp"
#include <pqxx/pqxx>
#include <memory>
#include <iostream>

namespace broker::adapters::secondary {

/**
 * @brief PostgreSQL repository for broker orders
 * 
 * BrokerOrder использует STRING поля для direction/orderType/status,
 * поэтому репозиторий работает напрямую со строками без конвертации.
 */
class PostgresBrokerOrderRepository : public ports::output::IBrokerOrderRepository {
public:
    explicit PostgresBrokerOrderRepository(std::shared_ptr<settings::DbSettings> settings)
        : settings_(std::move(settings))
    {
        // Схема создаётся в init.sql, не здесь
        std::cout << "[PostgresBrokerOrderRepository] Initialized" << std::endl;
    }

    std::vector<domain::BrokerOrder> findByAccountId(const std::string& accountId) override {
        std::vector<domain::BrokerOrder> orders;
        
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            auto result = txn.exec_params(
                "SELECT order_id, account_id, figi, direction, "
                "       quantity, filled_quantity, price, "
                "       order_type, status, reject_reason, received_at, updated_at "
                "FROM broker_orders WHERE account_id = $1 "
                "ORDER BY received_at DESC",
                accountId
            );
            
            for (const auto& row : result) {
                orders.push_back(rowToOrder(row));
            }
            
            txn.commit();
        } catch (const std::exception& e) {
            std::cerr << "[PostgresBrokerOrderRepository] findByAccountId error: " 
                      << e.what() << std::endl;
        }
        
        return orders;
    }

    std::optional<domain::BrokerOrder> findById(const std::string& orderId) override {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            auto result = txn.exec_params(
                "SELECT order_id, account_id, figi, direction, "
                "       quantity, filled_quantity, price, "
                "       order_type, status, reject_reason, received_at, updated_at "
                "FROM broker_orders WHERE order_id = $1",
                orderId
            );
            
            txn.commit();
            
            if (!result.empty()) {
                return rowToOrder(result[0]);
            }
        } catch (const std::exception& e) {
            std::cerr << "[PostgresBrokerOrderRepository] findById error: " 
                      << e.what() << std::endl;
        }
        
        return std::nullopt;
    }

    void save(const domain::BrokerOrder& order) override {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            // price в БД хранится в копейках (BIGINT)
            int64_t priceKopeks = static_cast<int64_t>(order.price * 100);
            
            txn.exec_params(
                "INSERT INTO broker_orders "
                "(order_id, account_id, figi, direction, quantity, filled_quantity, "
                " price, order_type, status, reject_reason, received_at, updated_at) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, NOW(), NOW()) "
                "ON CONFLICT (order_id) DO UPDATE SET "
                "filled_quantity = EXCLUDED.filled_quantity, "
                "status = EXCLUDED.status, "
                "reject_reason = EXCLUDED.reject_reason, "
                "updated_at = NOW()",
                order.orderId,
                order.accountId,
                order.figi,
                order.direction,
                order.requestedLots,
                order.executedLots,
                priceKopeks,
                order.orderType,
                order.status,
                ""  // reject_reason
            );
            
            txn.commit();
            std::cout << "[PostgresBrokerOrderRepository] Saved order: " << order.orderId << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[PostgresBrokerOrderRepository] save error: " << e.what() << std::endl;
            throw;
        }
    }

    void update(const domain::BrokerOrder& order) override {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            txn.exec_params(
                "UPDATE broker_orders SET "
                "filled_quantity = $1, status = $2, updated_at = NOW() "
                "WHERE order_id = $3",
                order.executedLots,
                order.status,
                order.orderId
            );
            
            txn.commit();
        } catch (const std::exception& e) {
            std::cerr << "[PostgresBrokerOrderRepository] update error: " << e.what() << std::endl;
            throw;
        }
    }

private:
    std::shared_ptr<settings::DbSettings> settings_;

    void initSchema() {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            txn.exec(R"(
                CREATE TABLE IF NOT EXISTS broker_orders (
                    order_id VARCHAR(64) PRIMARY KEY,
                    account_id VARCHAR(64) NOT NULL,
                    figi VARCHAR(32) NOT NULL,
                    direction VARCHAR(16) NOT NULL,
                    requested_lots BIGINT NOT NULL DEFAULT 0,
                    executed_lots BIGINT NOT NULL DEFAULT 0,
                    price DECIMAL(18,8) NOT NULL DEFAULT 0,
                    executed_price DECIMAL(18,8) NOT NULL DEFAULT 0,
                    order_type VARCHAR(16) NOT NULL,
                    status VARCHAR(32) NOT NULL,
                    created_at TIMESTAMP DEFAULT NOW(),
                    updated_at TIMESTAMP DEFAULT NOW()
                );
                
                CREATE INDEX IF NOT EXISTS idx_broker_orders_account 
                ON broker_orders(account_id);
                
                CREATE INDEX IF NOT EXISTS idx_broker_orders_status 
                ON broker_orders(status);
            )");
            
            txn.commit();
            std::cout << "[PostgresBrokerOrderRepository] Schema initialized" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[PostgresBrokerOrderRepository] initSchema error: " << e.what() << std::endl;
        }
    }

    domain::BrokerOrder rowToOrder(const pqxx::row& row) const {
        domain::BrokerOrder order;
        order.orderId = row["order_id"].as<std::string>();
        order.accountId = row["account_id"].as<std::string>();
        order.figi = row["figi"].as<std::string>();
        order.direction = row["direction"].as<std::string>();
        order.requestedLots = row["quantity"].as<int64_t>();
        order.executedLots = row["filled_quantity"].as<int64_t>();
        // price в БД в копейках, конвертируем в рубли
        order.price = row["price"].as<int64_t>() / 100.0;
        order.executedPrice = order.price;  // для совместимости
        order.orderType = row["order_type"].as<std::string>();
        order.status = row["status"].as<std::string>();
        order.createdAt = row["received_at"].is_null() ? "" : row["received_at"].as<std::string>();
        order.updatedAt = row["updated_at"].is_null() ? "" : row["updated_at"].as<std::string>();
        return order;
    }
};

} // namespace broker::adapters::secondary
