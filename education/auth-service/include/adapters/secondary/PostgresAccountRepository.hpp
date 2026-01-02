#pragma once

#include "ports/output/IAccountRepository.hpp"
#include "DbSettings.hpp"
#include <pqxx/pqxx>
#include <memory>
#include <mutex>
#include <iostream>

namespace auth::adapters::secondary {

class PostgresAccountRepository : public ports::output::IAccountRepository {
public:
    explicit PostgresAccountRepository(std::shared_ptr<DbSettings> settings)
        : settings_(std::move(settings))
    {
        std::cout << "[PostgresAccountRepository] Connecting..." << std::endl;
        try {
            connection_ = std::make_unique<pqxx::connection>(settings_->getConnectionString());
            std::cout << "[PostgresAccountRepository] Connected" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[PostgresAccountRepository] Connection failed: " << e.what() << std::endl;
            throw;
        }
    }

    ~PostgresAccountRepository() {
        if (connection_ && connection_->is_open()) {
            connection_->close();
        }
    }

    domain::Account save(const domain::Account& account) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            txn.exec_params(
                R"(
                    INSERT INTO accounts (account_id, user_id, name, type, tinkoff_token_encrypted, created_at)
                    VALUES ($1, $2, $3, $4, $5, NOW())
                    ON CONFLICT (account_id) DO UPDATE SET
                        name = EXCLUDED.name,
                        type = EXCLUDED.type,
                        tinkoff_token_encrypted = EXCLUDED.tinkoff_token_encrypted
                )",
                account.accountId,
                account.userId,
                account.name,
                domain::toString(account.type),
                account.tinkoffTokenEncrypted
            );
            
            txn.commit();
            return account;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresAccountRepository] save() failed: " << e.what() << std::endl;
            throw;
        }
    }

    std::optional<domain::Account> findById(const std::string& accountId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                R"(SELECT account_id, user_id, name, type, tinkoff_token_encrypted 
                   FROM accounts WHERE account_id = $1)",
                accountId
            );
            
            txn.commit();
            
            if (result.empty()) return std::nullopt;
            
            return rowToAccount(result[0]);
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresAccountRepository] findById() failed: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    std::vector<domain::Account> findByUserId(const std::string& userId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                R"(SELECT account_id, user_id, name, type, tinkoff_token_encrypted 
                   FROM accounts WHERE user_id = $1)",
                userId
            );
            
            txn.commit();
            
            std::vector<domain::Account> accounts;
            for (const auto& row : result) {
                accounts.push_back(rowToAccount(row));
            }
            return accounts;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresAccountRepository] findByUserId() failed: " << e.what() << std::endl;
            return {};
        }
    }

    bool deleteById(const std::string& accountId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                "DELETE FROM accounts WHERE account_id = $1",
                accountId
            );
            
            txn.commit();
            return result.affected_rows() > 0;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresAccountRepository] deleteById() failed: " << e.what() << std::endl;
            return false;
        }
    }

private:
    std::shared_ptr<DbSettings> settings_;
    std::unique_ptr<pqxx::connection> connection_;
    mutable std::mutex mutex_;

    domain::Account rowToAccount(const pqxx::row& row) const {
        return domain::Account(
            row["account_id"].as<std::string>(),
            row["user_id"].as<std::string>(),
            row["name"].as<std::string>(),
            domain::parseAccountType(row["type"].as<std::string>()),
            row["tinkoff_token_encrypted"].is_null() ? "" : row["tinkoff_token_encrypted"].as<std::string>()
        );
    }
};

} // namespace auth::adapters::secondary
