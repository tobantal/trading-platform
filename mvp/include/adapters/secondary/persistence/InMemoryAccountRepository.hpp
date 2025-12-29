// include/adapters/secondary/persistence/InMemoryAccountRepository.hpp
#pragma once

#include "ports/output/IAccountRepository.hpp"
#include <ThreadSafeMap.hpp>
#include <mutex>
#include <random>
#include <sstream>
#include <iomanip>
#include <set>
#include <iostream>


namespace trading::adapters::secondary {

/**
 * @brief In-memory реализация репозитория брокерских счетов
 * 
 * Тестовые аккаунты (привязаны к пользователям из InMemoryUserRepository):
 * ┌───────────────────────┬──────────┬───────────────────┬────────────┐
 * │ ID                    │ UserId   │ Name              │ Type       │
 * ├───────────────────────┼──────────┼───────────────────┼────────────┤
 * │ acc-001-sandbox       │ user-001 │ Sandbox Account   │ SANDBOX    │
 * │ acc-001-prod          │ user-001 │ Production        │ PRODUCTION │
 * │ acc-002-sandbox       │ user-002 │ My Sandbox        │ SANDBOX    │
 * │ acc-004-sandbox       │ user-004 │ Admin Sandbox     │ SANDBOX    │
 * └───────────────────────┴──────────┴───────────────────┴────────────┘
 * 
 * Примечание: user-003 (newbie) не имеет аккаунтов — для теста "добавить аккаунт"
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
        // ================================================================
        // ТЕСТОВЫЕ АККАУНТЫ
        // ID пользователей должны совпадать с InMemoryUserRepository!
        // ================================================================

        // ----------------------------------------------------------------
        // trader1 (user-001): 2 аккаунта — sandbox + production
        // Сценарий: опытный трейдер, переключается между средами
        // ----------------------------------------------------------------
        {
            domain::Account sandbox(
                "acc-001-sandbox",      // id
                "user-001",             // userId — совпадает с UserRepository!
                "Sandbox Account",      // name
                domain::AccountType::SANDBOX,
                "",                     // accessToken
                true                    // active
            );
            save(sandbox);
        }
        {
            domain::Account prod(
                "acc-001-prod",
                "user-001",
                "Production Account",
                domain::AccountType::PRODUCTION,
                "",
                true
            );
            save(prod);
        }

        // ----------------------------------------------------------------
        // trader2 (user-002): 1 аккаунт — только sandbox
        // Сценарий: начинающий трейдер
        // ----------------------------------------------------------------
        {
            domain::Account sandbox(
                "acc-002-sandbox",
                "user-002",             // userId — совпадает с UserRepository!
                "My Sandbox",
                domain::AccountType::SANDBOX,
                "",
                true
            );
            save(sandbox);
        }

        // ----------------------------------------------------------------
        // newbie (user-003): 0 аккаунтов
        // Сценарий: новый пользователь, тест "добавить первый аккаунт"
        // ----------------------------------------------------------------
        // Ничего не создаём!

        // ----------------------------------------------------------------
        // admin (user-004): 1 аккаунт — sandbox
        // Сценарий: администратор для тестирования
        // ----------------------------------------------------------------
        {
            domain::Account sandbox(
                "acc-004-sandbox",
                "user-004",             // userId — совпадает с UserRepository!
                "Admin Sandbox",
                domain::AccountType::SANDBOX,
                "",
                true
            );
            save(sandbox);
        }

        std::cout << "[InMemoryAccountRepository] Initialized 4 test accounts for 3 users" << std::endl;
        std::cout << "  - user-001 (trader1): 2 accounts (sandbox + prod)" << std::endl;
        std::cout << "  - user-002 (trader2): 1 account (sandbox)" << std::endl;
        std::cout << "  - user-003 (newbie):  0 accounts" << std::endl;
        std::cout << "  - user-004 (admin):   1 account (sandbox)" << std::endl;
    }
};

} // namespace trading::adapters::secondary
