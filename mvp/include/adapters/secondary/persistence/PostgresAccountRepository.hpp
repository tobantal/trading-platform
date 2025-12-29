#pragma once

#include "ports/output/IAccountRepository.hpp"
#include <pqxx/pqxx>
#include <mutex>
#include <memory>
#include <iostream>

namespace trading::adapters::secondary {

/**
 * @brief PostgreSQL реализация репозитория брокерских счетов
 */
class PostgresAccountRepository : public ports::output::IAccountRepository {
public:
    /**
     * @brief Конструктор с connection string
     */
    explicit PostgresAccountRepository(const std::string& connectionString)
        : connectionString_(connectionString)
    {
        std::cout << "[PostgresAccountRepo] Connecting to PostgreSQL..." << std::endl;
        try {
            connection_ = std::make_unique<pqxx::connection>(connectionString);
            std::cout << "[PostgresAccountRepo] Connected successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[PostgresAccountRepo] Connection failed: " << e.what() << std::endl;
            throw;
        }
    }

    ~PostgresAccountRepository() override {
        if (connection_ && connection_->is_open()) {
            connection_->close();
        }
    }

    /**
     * @brief Сохранить счёт
     */
    void save(const domain::Account& account) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            txn.exec_params(
                R"(
                    INSERT INTO accounts (id, user_id, name, type, broker_account_id, broker_token, is_active)
                    VALUES ($1, $2, $3, $4, $5, $6, $7)
                    ON CONFLICT (id) DO UPDATE SET
                        name = EXCLUDED.name,
                        type = EXCLUDED.type,
                        broker_account_id = EXCLUDED.broker_account_id,
                        broker_token = EXCLUDED.broker_token,
                        is_active = EXCLUDED.is_active,
                        updated_at = NOW()
                )",
                account.id,
                account.userId,
                account.name,
                accountTypeToString(account.type),
                account.brokerAccountId,
                account.accessToken,
                account.isActive
            );
            
            txn.commit();
            std::cout << "[PostgresAccountRepo] Saved account: " << account.name << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresAccountRepo] save() failed: " << e.what() << std::endl;
            throw;
        }
    }

    /**
     * @brief Найти счёт по ID
     */
    std::optional<domain::Account> findById(const std::string& id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                R"(
                    SELECT id, user_id, name, type, broker_account_id, broker_token, is_active
                    FROM accounts WHERE id = $1
                )",
                id
            );
            
            txn.commit();
            
            if (result.empty()) {
                return std::nullopt;
            }
            
            return rowToAccount(result[0]);
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresAccountRepo] findById() failed: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    /**
     * @brief Найти все счета пользователя
     */
    std::vector<domain::Account> findByUserId(const std::string& userId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<domain::Account> accounts;
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                R"(
                    SELECT id, user_id, name, type, broker_account_id, broker_token, is_active
                    FROM accounts WHERE user_id = $1
                    ORDER BY created_at ASC
                )",
                userId
            );
            
            txn.commit();
            
            for (const auto& row : result) {
                accounts.push_back(rowToAccount(row));
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresAccountRepo] findByUserId() failed: " << e.what() << std::endl;
        }
        
        return accounts;
    }

    /**
     * @brief Обновить счёт
     */
    void update(const domain::Account& account) override {
        save(account);  // UPSERT в save() обновит существующий
    }

    /**
     * @brief Удалить счёт
     */
    bool deleteById(const std::string& id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                "DELETE FROM accounts WHERE id = $1",
                id
            );
            
            txn.commit();
            
            return result.affected_rows() > 0;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresAccountRepo] deleteById() failed: " << e.what() << std::endl;
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
     * @brief Преобразовать строку результата в объект Account
     */
    domain::Account rowToAccount(const pqxx::row& row) const {
        domain::Account account;
        account.id = row["id"].as<std::string>();
        account.userId = row["user_id"].as<std::string>();
        account.name = row["name"].as<std::string>();
        account.type = stringToAccountType(row["type"].as<std::string>());
        
        if (!row["broker_account_id"].is_null()) {
            account.brokerAccountId = row["broker_account_id"].as<std::string>();
        }
        if (!row["broker_token"].is_null()) {
            account.accessToken = row["broker_token"].as<std::string>();
        }
        
        account.isActive = row["is_active"].as<bool>();
        return account;
    }

    /**
     * @brief AccountType → string
     */
    std::string accountTypeToString(domain::AccountType type) const {
        switch (type) {
            case domain::AccountType::SANDBOX: return "SANDBOX";
            case domain::AccountType::PRODUCTION: return "PRODUCTION";
            default: return "SANDBOX";
        }
    }

    /**
     * @brief string → AccountType
     */
    domain::AccountType stringToAccountType(const std::string& str) const {
        if (str == "PRODUCTION") return domain::AccountType::PRODUCTION;
        return domain::AccountType::SANDBOX;
    }

    std::string connectionString_;
    std::unique_ptr<pqxx::connection> connection_;
    mutable std::mutex mutex_;
};

} // namespace trading::adapters::secondary
