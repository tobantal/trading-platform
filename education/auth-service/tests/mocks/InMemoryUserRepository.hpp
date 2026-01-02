#pragma once

#include "ports/output/IUserRepository.hpp"
#include <unordered_map>
#include <mutex>

namespace auth::tests::mocks {

/**
 * @brief In-Memory реализация репозитория пользователей для unit-тестов
 */
class InMemoryUserRepository : public ports::output::IUserRepository {
public:
    domain::User save(const domain::User& user) override {
        std::lock_guard<std::mutex> lock(mutex_);
        users_[user.userId] = user;
        usernameIndex_[user.username] = user.userId;
        emailIndex_[user.email] = user.userId;
        return user;
    }

    std::optional<domain::User> findById(const std::string& userId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = users_.find(userId);
        if (it == users_.end()) return std::nullopt;
        return it->second;
    }

    std::optional<domain::User> findByUsername(const std::string& username) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = usernameIndex_.find(username);
        if (it == usernameIndex_.end()) return std::nullopt;
        return users_[it->second];
    }

    std::optional<domain::User> findByEmail(const std::string& email) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = emailIndex_.find(email);
        if (it == emailIndex_.end()) return std::nullopt;
        return users_[it->second];
    }

    bool existsByUsername(const std::string& username) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return usernameIndex_.count(username) > 0;
    }

    bool existsByEmail(const std::string& email) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return emailIndex_.count(email) > 0;
    }

    // Test helpers
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        users_.clear();
        usernameIndex_.clear();
        emailIndex_.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return users_.size();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, domain::User> users_;
    std::unordered_map<std::string, std::string> usernameIndex_;  // username -> userId
    std::unordered_map<std::string, std::string> emailIndex_;     // email -> userId
};

} // namespace auth::tests::mocks
