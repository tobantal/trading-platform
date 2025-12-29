#pragma once

#include "ports/output/IUserRepository.hpp"
#include <pqxx/pqxx>
#include <mutex>
#include <memory>
#include <iostream>

namespace trading::adapters::secondary {

/**
 * @brief PostgreSQL реализация репозитория пользователей
 * 
 * Использует libpqxx для взаимодействия с PostgreSQL.
 * Thread-safe благодаря мьютексу на соединение.
 * 
 * Зависимости:
 * - libpqxx (CMake: find_package(libpqxx REQUIRED))
 * - PostgreSQL 15+
 */
class PostgresUserRepository : public ports::output::IUserRepository {
public:
    /**
     * @brief Конструктор с connection string
     * @param connectionString PostgreSQL connection string
     *        Формат: "host=localhost port=5432 dbname=trading user=trader password=password"
     */
    explicit PostgresUserRepository(const std::string& connectionString)
        : connectionString_(connectionString)
    {
        std::cout << "[PostgresUserRepo] Connecting to PostgreSQL..." << std::endl;
        try {
            connection_ = std::make_unique<pqxx::connection>(connectionString);
            std::cout << "[PostgresUserRepo] Connected successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[PostgresUserRepo] Connection failed: " << e.what() << std::endl;
            throw;
        }
    }

    ~PostgresUserRepository() override {
        if (connection_ && connection_->is_open()) {
            connection_->close();
        }
    }

    /**
     * @brief Сохранить пользователя (INSERT или UPDATE)
     */
    void save(const domain::User& user) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            // UPSERT: INSERT ... ON CONFLICT DO UPDATE
            txn.exec_params(
                R"(
                    INSERT INTO users (id, username, password_hash, created_at, updated_at)
                    VALUES ($1, $2, $3, NOW(), NOW())
                    ON CONFLICT (id) DO UPDATE SET
                        username = EXCLUDED.username,
                        password_hash = EXCLUDED.password_hash,
                        updated_at = NOW()
                )",
                user.id,
                user.username,
                user.passwordHash
            );
            
            txn.commit();
            std::cout << "[PostgresUserRepo] Saved user: " << user.username << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresUserRepo] save() failed: " << e.what() << std::endl;
            throw;
        }
    }

    /**
     * @brief Найти пользователя по ID
     */
    std::optional<domain::User> findById(const std::string& id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                "SELECT id, username, password_hash FROM users WHERE id = $1",
                id
            );
            
            txn.commit();
            
            if (result.empty()) {
                return std::nullopt;
            }
            
            return rowToUser(result[0]);
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresUserRepo] findById() failed: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    /**
     * @brief Найти пользователя по username
     */
    std::optional<domain::User> findByUsername(const std::string& username) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                "SELECT id, username, password_hash FROM users WHERE username = $1",
                username
            );
            
            txn.commit();
            
            if (result.empty()) {
                return std::nullopt;
            }
            
            return rowToUser(result[0]);
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresUserRepo] findByUsername() failed: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    /**
     * @brief Удалить пользователя по ID
     */
    bool deleteById(const std::string& id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                "DELETE FROM users WHERE id = $1",
                id
            );
            
            txn.commit();
            
            return result.affected_rows() > 0;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresUserRepo] deleteById() failed: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Проверить существование пользователя по username
     */
    bool existsByUsername(const std::string& username) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                "SELECT EXISTS(SELECT 1 FROM users WHERE username = $1)",
                username
            );
            
            txn.commit();
            
            return result[0][0].as<bool>();
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresUserRepo] existsByUsername() failed: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Проверить соединение с БД
     */
    bool isConnected() const {
        return connection_ && connection_->is_open();
    }

    /**
     * @brief Переподключиться к БД
     */
    void reconnect() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (connection_) {
            connection_->close();
        }
        
        connection_ = std::make_unique<pqxx::connection>(connectionString_);
    }

private:
    /**
     * @brief Преобразовать строку результата в объект User
     */
    domain::User rowToUser(const pqxx::row& row) const {
        domain::User user;
        user.id = row["id"].as<std::string>();
        user.username = row["username"].as<std::string>();
        user.passwordHash = row["password_hash"].as<std::string>();
        return user;
    }

    std::string connectionString_;
    std::unique_ptr<pqxx::connection> connection_;
    mutable std::mutex mutex_;
};

} // namespace trading::adapters::secondary
