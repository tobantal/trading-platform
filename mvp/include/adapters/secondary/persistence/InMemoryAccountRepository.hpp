#pragma once

#include "ports/output/IAccountRepository.hpp"
#include <ThreadSafeMap.hpp>
#include <mutex>
#include <random>
#include <sstream>
#include <iomanip>
#include <set>


namespace trading::adapters::secondary {

/**
 * @brief In-memory реализация репозитория брокерских счетов
 */
class InMemoryAccountRepository : public ports::output::IAccountRepository {
public:
    InMemoryAccountRepository() : rng_(std::random_device{}()) {
        initTestAccounts();
    }

    /**
     * @brief Сохранить счёт
     */
    void save(const domain::Account& account) override {
        accounts_.insert(account.id, std::make_shared<domain::Account>(account));
        
        std::lock_guard<std::mutex> lock(indexMutex_);
        userAccounts_[account.userId].insert(account.id);
    }

    /**
     * @brief Найти счёт по ID
     */
    std::optional<domain::Account> findById(const std::string& id) override {
        auto account = accounts_.find(id);
        return account ? std::optional(*account) : std::nullopt;
    }

    /**
     * @brief Найти все счета пользователя
     */
    std::vector<domain::Account> findByUserId(const std::string& userId) override {
        std::vector<domain::Account> result;
        
        std::set<std::string> accountIds;
        {
            std::lock_guard<std::mutex> lock(indexMutex_);
            auto it = userAccounts_.find(userId);
            if (it != userAccounts_.end()) {
                accountIds = it->second;
            }
        }
        
        for (const auto& id : accountIds) {
            if (auto account = accounts_.find(id)) {
                result.push_back(*account);
            }
        }
        
        return result;
    }

    /**
     * @brief Обновить счёт
     */
    void update(const domain::Account& account) override {
        auto existing = accounts_.find(account.id);
        if (existing) {
            accounts_.insert(account.id, std::make_shared<domain::Account>(account));
        }
    }

    /**
     * @brief Удалить счёт
     */
    bool deleteById(const std::string& id) override {
        auto account = accounts_.find(id);
        if (!account) {
            return false;
        }
        
        {
            std::lock_guard<std::mutex> lock(indexMutex_);
            userAccounts_[account->userId].erase(id);
            if (activeAccounts_[account->userId] == id) {
                activeAccounts_.erase(account->userId);
            }
        }
        
        accounts_.remove(id);
        return true;
    }

    /**
     * @brief Установить активный счёт для пользователя
     */
    void setActiveAccount(const std::string& userId, const std::string& accountId) override {
        auto account = accounts_.find(accountId);
        if (!account || account->userId != userId) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(indexMutex_);
        activeAccounts_[userId] = accountId;
    }

    /**
     * @brief Найти активный счёт пользователя
     */
    std::optional<domain::Account> findActiveByUserId(const std::string& userId) override {
        std::string accountId;
        {
            std::lock_guard<std::mutex> lock(indexMutex_);
            auto it = activeAccounts_.find(userId);
            if (it == activeAccounts_.end()) {
                // Если нет активного, берём первый
                auto accIt = userAccounts_.find(userId);
                if (accIt == userAccounts_.end() || accIt->second.empty()) {
                    return std::nullopt;
                }
                accountId = *accIt->second.begin();
            } else {
                accountId = it->second;
            }
        }
        return findById(accountId);
    }

    /**
     * @brief Создать счёт для пользователя (удобный метод)
     */
    domain::Account createForUser(const std::string& userId, const std::string& name, 
                                   domain::AccountType type) {
        domain::Account account(
            generateUuid(),
            userId,
            name,
            type,
            "",  // accessToken
            true // isActive
        );
        save(account);
        return account;
    }

    /**
     * @brief Очистить репозиторий
     */
    void clear() {
        accounts_.clear();
        std::lock_guard<std::mutex> lock(indexMutex_);
        userAccounts_.clear();
        activeAccounts_.clear();
    }

private:
    ThreadSafeMap<std::string, domain::Account> accounts_;
    mutable std::mutex indexMutex_;
    std::unordered_map<std::string, std::set<std::string>> userAccounts_; // userId -> accountIds
    std::unordered_map<std::string, std::string> activeAccounts_; // userId -> activeAccountId
    std::mt19937_64 rng_;

    std::string generateUuid() {
        std::uniform_int_distribution<uint64_t> dist;
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        ss << std::setw(8) << (dist(rng_) & 0xFFFFFFFF) << "-";
        ss << std::setw(4) << (dist(rng_) & 0xFFFF) << "-";
        ss << std::setw(4) << ((dist(rng_) & 0x0FFF) | 0x4000) << "-";
        ss << std::setw(4) << ((dist(rng_) & 0x3FFF) | 0x8000) << "-";
        ss << std::setw(12) << (dist(rng_) & 0xFFFFFFFFFFFF);
        return ss.str();
    }

    void initTestAccounts() {
        // Тестовые пользователи создаются в InMemoryUserRepository
        // Здесь создаём sandbox счета для них

        // trader1
        {
            domain::Account acc(
                "acc-trader1-sandbox",
                "user-trader1",
                "Sandbox Trader1",
                domain::AccountType::SANDBOX,
                "",
                true
            );
            save(acc);
            setActiveAccount("user-trader1", acc.id);
        }

        // trader2
        {
            domain::Account acc(
                "acc-trader2-sandbox",
                "user-trader2",
                "Sandbox Trader2",
                domain::AccountType::SANDBOX,
                "",
                true
            );
            save(acc);
            setActiveAccount("user-trader2", acc.id);
        }

        // admin
        {
            domain::Account acc(
                "acc-admin-sandbox",
                "user-admin",
                "Sandbox Admin",
                domain::AccountType::SANDBOX,
                "",
                true
            );
            save(acc);
            setActiveAccount("user-admin", acc.id);
        }
    }
};

} // namespace trading::adapters::secondary
