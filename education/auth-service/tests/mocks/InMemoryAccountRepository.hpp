#pragma once

#include "ports/output/IAccountRepository.hpp"
#include <unordered_map>
#include <mutex>
#include <algorithm>

namespace auth::tests::mocks {

/**
 * @brief In-Memory реализация репозитория аккаунтов для unit-тестов
 */
class InMemoryAccountRepository : public ports::output::IAccountRepository {
public:
    domain::Account save(const domain::Account& account) override {
        std::lock_guard<std::mutex> lock(mutex_);
        accounts_[account.accountId] = account;
        return account;
    }

    std::optional<domain::Account> findById(const std::string& accountId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = accounts_.find(accountId);
        if (it == accounts_.end()) return std::nullopt;
        return it->second;
    }

    std::vector<domain::Account> findByUserId(const std::string& userId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<domain::Account> result;
        for (const auto& [id, account] : accounts_) {
            if (account.userId == userId) {
                result.push_back(account);
            }
        }
        return result;
    }

    bool deleteById(const std::string& accountId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return accounts_.erase(accountId) > 0;
    }

    // Test helpers
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        accounts_.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return accounts_.size();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, domain::Account> accounts_;
};

} // namespace auth::tests::mocks
