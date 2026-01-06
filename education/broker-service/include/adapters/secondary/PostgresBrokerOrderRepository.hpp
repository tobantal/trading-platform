// broker-service/include/adapters/secondary/PostgresBrokerOrderRepository.hpp
#pragma once

#include "ports/output/IBrokerOrderRepository.hpp"
#include "settings/DbSettings.hpp"
#include <pqxx/pqxx>
#include <memory>
#include <iostream>
#include <vector>
#include <optional>

namespace broker::adapters::secondary {

class PostgresBrokerOrderRepository : public ports::output::IBrokerOrderRepository {
public:
    explicit PostgresBrokerOrderRepository(std::shared_ptr<settings::DbSettings> settings)
        : settings_(std::move(settings))
    {
        ensureExecutedPriceColumn();
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
                "       COALESCE(executed_price, price) as executed_price, "
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
                "       COALESCE(executed_price, price) as executed_price, "
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
            
            txn.exec_params(
                "INSERT INTO broker_orders "
                "(order_id, account_id, figi, direction, quantity, filled_quantity, "
                " price, executed_price, order_type, status, reject_reason, received_at, updated_at) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, NOW(), NOW()) "
                "ON CONFLICT (order_id) DO UPDATE SET "
                "filled_quantity = EXCLUDED.filled_quantity, "
                "executed_price = EXCLUDED.executed_price, "
                "status = EXCLUDED.status, "
                "reject_reason = EXCLUDED.reject_reason, "
                "updated_at = NOW()",
                order.orderId,
                order.accountId,
                order.figi,
                order.direction,
                order.requestedLots,
                order.executedLots,
                order.price,
                order.executedPrice,
                order.orderType,
                order.status,
                ""  // reject_reason
            );
            
            txn.commit();
            std::cout << "[PostgresBrokerOrderRepository] Saved order: " << order.orderId 
                      << " executed_price=" << order.executedPrice << std::endl;
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
                "filled_quantity = $1, executed_price = $2, status = $3, updated_at = NOW() "
                "WHERE order_id = $4",
                order.executedLots,
                order.executedPrice,
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

    /**
     * @brief Добавляет колонку executed_price если её нет (миграция)
     */
    void ensureExecutedPriceColumn() {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            // Добавляем колонку если её нет
            txn.exec(R"(
                ALTER TABLE broker_orders 
                ADD COLUMN IF NOT EXISTS executed_price DECIMAL(18,8) NOT NULL DEFAULT 0
            )");
            
            txn.commit();
            std::cout << "[PostgresBrokerOrderRepository] Ensured executed_price column exists" << std::endl;
        } catch (const std::exception& e) {
            // Игнорируем ошибку если колонка уже есть
            std::cerr << "[PostgresBrokerOrderRepository] ensureExecutedPriceColumn: " 
                      << e.what() << std::endl;
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
        order.price = row["price"].as<double>();
        order.executedPrice = row["executed_price"].as<double>();
        order.orderType = row["order_type"].as<std::string>();
        order.status = row["status"].as<std::string>();
        order.createdAt = row["received_at"].is_null() ? "" : row["received_at"].as<std::string>();
        order.updatedAt = row["updated_at"].is_null() ? "" : row["updated_at"].as<std::string>();
        return order;
    }
};

} // namespace broker::adapters::secondary
