#pragma once

#include "ports/output/IOrderRepository.hpp"
#include <pqxx/pqxx>
#include <mutex>
#include <memory>
#include <iostream>

namespace trading::adapters::secondary {

/**
 * @brief PostgreSQL реализация репозитория ордеров
 */
class PostgresOrderRepository : public ports::output::IOrderRepository {
public:
    /**
     * @brief Конструктор с connection string
     */
    explicit PostgresOrderRepository(const std::string& connectionString)
        : connectionString_(connectionString)
    {
        std::cout << "[PostgresOrderRepo] Connecting to PostgreSQL..." << std::endl;
        try {
            connection_ = std::make_unique<pqxx::connection>(connectionString);
            std::cout << "[PostgresOrderRepo] Connected successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[PostgresOrderRepo] Connection failed: " << e.what() << std::endl;
            throw;
        }
    }

    ~PostgresOrderRepository() override {
        if (connection_ && connection_->is_open()) {
            connection_->close();
        }
    }

    /**
     * @brief Сохранить ордер
     */
    void save(const domain::Order& order) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            txn.exec_params(
                R"(
                    INSERT INTO orders (
                        id, account_id, figi, direction, order_type, quantity,
                        price_units, price_nano, price_currency, status,
                        executed_quantity, executed_price_units, executed_price_nano,
                        broker_order_id, reject_reason, created_at
                    )
                    VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16)
                    ON CONFLICT (id) DO UPDATE SET
                        status = EXCLUDED.status,
                        executed_quantity = EXCLUDED.executed_quantity,
                        executed_price_units = EXCLUDED.executed_price_units,
                        executed_price_nano = EXCLUDED.executed_price_nano,
                        broker_order_id = EXCLUDED.broker_order_id,
                        reject_reason = EXCLUDED.reject_reason,
                        updated_at = NOW(),
                        executed_at = CASE 
                            WHEN EXCLUDED.status IN ('FILLED', 'PARTIALLY_FILLED') THEN NOW()
                            ELSE orders.executed_at
                        END
                )",
                order.id,
                order.accountId,
                order.figi,
                directionToString(order.direction),
                orderTypeToString(order.type),
                order.quantity,
                order.price.units,
                order.price.nano,
                order.price.currency,
                statusToString(order.status),
                order.executedQuantity,
                order.executedPrice.units,
                order.executedPrice.nano,
                order.brokerOrderId,
                order.rejectReason,
                timestampToString(order.createdAt)
            );
            
            txn.commit();
            std::cout << "[PostgresOrderRepo] Saved order: " << order.id << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresOrderRepo] save() failed: " << e.what() << std::endl;
            throw;
        }
    }

    /**
     * @brief Найти ордер по ID
     */
    std::optional<domain::Order> findById(const std::string& orderId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                R"(
                    SELECT id, account_id, figi, direction, order_type, quantity,
                           price_units, price_nano, price_currency, status,
                           executed_quantity, executed_price_units, executed_price_nano,
                           broker_order_id, reject_reason, created_at
                    FROM orders WHERE id = $1
                )",
                orderId
            );
            
            txn.commit();
            
            if (result.empty()) {
                return std::nullopt;
            }
            
            return rowToOrder(result[0]);
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresOrderRepo] findById() failed: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    /**
     * @brief Найти все ордера аккаунта
     */
    std::vector<domain::Order> findByAccountId(const std::string& accountId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<domain::Order> orders;
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                R"(
                    SELECT id, account_id, figi, direction, order_type, quantity,
                           price_units, price_nano, price_currency, status,
                           executed_quantity, executed_price_units, executed_price_nano,
                           broker_order_id, reject_reason, created_at
                    FROM orders 
                    WHERE account_id = $1
                    ORDER BY created_at DESC
                )",
                accountId
            );
            
            txn.commit();
            
            for (const auto& row : result) {
                orders.push_back(rowToOrder(row));
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresOrderRepo] findByAccountId() failed: " << e.what() << std::endl;
        }
        
        return orders;
    }

    /**
     * @brief Найти активные ордера аккаунта
     */
    std::vector<domain::Order> findActiveByAccountId(const std::string& accountId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<domain::Order> orders;
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                R"(
                    SELECT id, account_id, figi, direction, order_type, quantity,
                           price_units, price_nano, price_currency, status,
                           executed_quantity, executed_price_units, executed_price_nano,
                           broker_order_id, reject_reason, created_at
                    FROM orders 
                    WHERE account_id = $1 AND status IN ('NEW', 'PENDING', 'PARTIALLY_FILLED')
                    ORDER BY created_at DESC
                )",
                accountId
            );
            
            txn.commit();
            
            for (const auto& row : result) {
                orders.push_back(rowToOrder(row));
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresOrderRepo] findActiveByAccountId() failed: " << e.what() << std::endl;
        }
        
        return orders;
    }

    /**
     * @brief Обновить статус ордера
     */
    void updateStatus(const std::string& orderId, domain::OrderStatus status) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            txn.exec_params(
                R"(
                    UPDATE orders SET 
                        status = $2,
                        updated_at = NOW(),
                        executed_at = CASE WHEN $2 IN ('FILLED', 'PARTIALLY_FILLED') THEN NOW() ELSE executed_at END
                    WHERE id = $1
                )",
                orderId,
                statusToString(status)
            );
            
            txn.commit();
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresOrderRepo] updateStatus() failed: " << e.what() << std::endl;
            throw;
        }
    }

    /**
     * @brief Удалить ордер
     */
    bool deleteById(const std::string& orderId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                "DELETE FROM orders WHERE id = $1",
                orderId
            );
            
            txn.commit();
            
            return result.affected_rows() > 0;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresOrderRepo] deleteById() failed: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Проверить соединение с БД
     */
    bool isConnected() const {
        return connection_ && connection_->is_open();
    }

private:
    /**
     * @brief Преобразовать строку результата в объект Order
     */
    domain::Order rowToOrder(const pqxx::row& row) const {
        domain::Order order;
        order.id = row["id"].as<std::string>();
        order.accountId = row["account_id"].as<std::string>();
        order.figi = row["figi"].as<std::string>();
        order.direction = stringToDirection(row["direction"].as<std::string>());
        order.type = stringToOrderType(row["order_type"].as<std::string>());
        order.quantity = row["quantity"].as<int64_t>();
        
        order.price.units = row["price_units"].as<int64_t>();
        order.price.nano = row["price_nano"].as<int32_t>();
        order.price.currency = row["price_currency"].as<std::string>();
        
        order.status = stringToStatus(row["status"].as<std::string>());
        order.executedQuantity = row["executed_quantity"].as<int64_t>();
        
        order.executedPrice.units = row["executed_price_units"].as<int64_t>();
        order.executedPrice.nano = row["executed_price_nano"].as<int32_t>();
        
        if (!row["broker_order_id"].is_null()) {
            order.brokerOrderId = row["broker_order_id"].as<std::string>();
        }
        if (!row["reject_reason"].is_null()) {
            order.rejectReason = row["reject_reason"].as<std::string>();
        }
        
        // TODO: Parse created_at timestamp
        
        return order;
    }

    // Конвертеры enum <-> string
    std::string directionToString(domain::OrderDirection dir) const {
        return dir == domain::OrderDirection::BUY ? "BUY" : "SELL";
    }
    
    domain::OrderDirection stringToDirection(const std::string& s) const {
        return s == "BUY" ? domain::OrderDirection::BUY : domain::OrderDirection::SELL;
    }
    
    std::string orderTypeToString(domain::OrderType type) const {
        switch (type) {
            case domain::OrderType::MARKET: return "MARKET";
            case domain::OrderType::LIMIT: return "LIMIT";
            case domain::OrderType::STOP: return "STOP";
            case domain::OrderType::STOP_LIMIT: return "STOP_LIMIT";
            default: return "MARKET";
        }
    }
    
    domain::OrderType stringToOrderType(const std::string& s) const {
        if (s == "LIMIT") return domain::OrderType::LIMIT;
        if (s == "STOP") return domain::OrderType::STOP;
        if (s == "STOP_LIMIT") return domain::OrderType::STOP_LIMIT;
        return domain::OrderType::MARKET;
    }
    
    std::string statusToString(domain::OrderStatus status) const {
        switch (status) {
            case domain::OrderStatus::NEW: return "NEW";
            case domain::OrderStatus::PENDING: return "PENDING";
            case domain::OrderStatus::FILLED: return "FILLED";
            case domain::OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
            case domain::OrderStatus::CANCELLED: return "CANCELLED";
            case domain::OrderStatus::REJECTED: return "REJECTED";
            case domain::OrderStatus::EXPIRED: return "EXPIRED";
            default: return "NEW";
        }
    }
    
    domain::OrderStatus stringToStatus(const std::string& s) const {
        if (s == "PENDING") return domain::OrderStatus::PENDING;
        if (s == "FILLED") return domain::OrderStatus::FILLED;
        if (s == "PARTIALLY_FILLED") return domain::OrderStatus::PARTIALLY_FILLED;
        if (s == "CANCELLED") return domain::OrderStatus::CANCELLED;
        if (s == "REJECTED") return domain::OrderStatus::REJECTED;
        if (s == "EXPIRED") return domain::OrderStatus::EXPIRED;
        return domain::OrderStatus::NEW;
    }
    
    std::string timestampToString(std::chrono::system_clock::time_point tp) const {
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    std::string connectionString_;
    std::unique_ptr<pqxx::connection> connection_;
    mutable std::mutex mutex_;
};

} // namespace trading::adapters::secondary
