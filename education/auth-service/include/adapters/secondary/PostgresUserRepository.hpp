#pragma once

#include "ports/output/IUserRepository.hpp"
#include "DbSettings.hpp"
#include <pqxx/pqxx>
#include <memory>
#include <mutex>
#include <iostream>

namespace auth::adapters::secondary {

/**
 * @brief PostgreSQL репозиторий пользователей
 */
class PostgresUserRepository : public ports::output::IUserRepository {
public:
    explicit PostgresUserRepository(std::shared_ptr<DbSettings> settings)
        : settings_(std::move(settings))
    {
        std::cout << "[PostgresUserRepository] Connecting to " << settings_->getHost() << std::endl;
        try {
            connection_ = std::make_unique<pqxx::connection>(settings_->getConnectionString());
            std::cout << "[PostgresUserRepository] Connected successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[PostgresUserRepository] Connection failed: " << e.what() << std::endl;
            throw;
        }
    }

    ~PostgresUserRepository() {
        if (connection_ && connection_->is_open()) {
            connection_->close();
        }
    }

    domain::User save(const domain::User& user) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            txn.exec_params(
                R"(
                    INSERT INTO users (user_id, username, email, password_hash, created_at)
                    VALUES ($1, $2, $3, $4, NOW())
                    ON CONFLICT (user_id) DO UPDATE SET
                        username = EXCLUDED.username,
                        email = EXCLUDED.email,
                        password_hash = EXCLUDED.password_hash
                )",
                user.userId,
                user.username,
                user.email,
                user.passwordHash
            );
            
            txn.commit();
            return user;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresUserRepository] save() failed: " << e.what() << std::endl;
            throw;
        }
    }

    std::optional<domain::User> findById(const std::string& userId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                "SELECT user_id, username, email, password_hash FROM users WHERE user_id = $1",
                userId
            );
            
            txn.commit();
            
            if (result.empty()) return std::nullopt;
            
            return rowToUser(result[0]);
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresUserRepository] findById() failed: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    std::optional<domain::User> findByUsername(const std::string& username) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                "SELECT user_id, username, email, password_hash FROM users WHERE username = $1",
                username
            );
            
            txn.commit();
            
            if (result.empty()) return std::nullopt;
            
            return rowToUser(result[0]);
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresUserRepository] findByUsername() failed: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    std::optional<domain::User> findByEmail(const std::string& email) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                "SELECT user_id, username, email, password_hash FROM users WHERE email = $1",
                email
            );
            
            txn.commit();
            
            if (result.empty()) return std::nullopt;
            
            return rowToUser(result[0]);
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresUserRepository] findByEmail() failed: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    bool existsByUsername(const std::string& username) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                "SELECT 1 FROM users WHERE username = $1 LIMIT 1",
                username
            );
            
            txn.commit();
            return !result.empty();
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresUserRepository] existsByUsername() failed: " << e.what() << std::endl;
            return false;
        }
    }

    bool existsByEmail(const std::string& email) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                "SELECT 1 FROM users WHERE email = $1 LIMIT 1",
                email
            );
            
            txn.commit();
            return !result.empty();
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresUserRepository] existsByEmail() failed: " << e.what() << std::endl;
            return false;
        }
    }

private:
    std::shared_ptr<DbSettings> settings_;
    std::unique_ptr<pqxx::connection> connection_;
    mutable std::mutex mutex_;

    domain::User rowToUser(const pqxx::row& row) const {
        return domain::User(
            row["user_id"].as<std::string>(),
            row["username"].as<std::string>(),
            row["email"].as<std::string>(),
            row["password_hash"].as<std::string>()
        );
    }
};

} // namespace auth::adapters::secondary
