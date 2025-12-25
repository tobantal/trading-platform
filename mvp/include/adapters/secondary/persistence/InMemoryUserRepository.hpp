#pragma once

#include "ports/output/IUserRepository.hpp"
#include <ThreadSafeMap.hpp>
#include <mutex>
#include <random>
#include <sstream>
#include <iomanip>

namespace trading::adapters::secondary {

/**
 * @brief In-memory реализация репозитория пользователей
 * 
 * Для MVP используем хранение в памяти.
 * В Education заменяется на PostgresUserRepository.
 */
class InMemoryUserRepository : public ports::output::IUserRepository {
public:
    InMemoryUserRepository() : rng_(std::random_device{}()) {
        // Создаём тестовых пользователей
        initTestUsers();
    }

    /**
     * @brief Сохранить пользователя
     */
    void save(const domain::User& user) override {
        users_.insert(user.id, std::make_shared<domain::User>(user));
        
        std::lock_guard<std::mutex> lock(indexMutex_);
        usernameIndex_[user.username] = user.id;
    }

    /**
     * @brief Найти пользователя по ID
     */
    std::optional<domain::User> findById(const std::string& id) override {
        auto user = users_.find(id);
        return user ? std::optional(*user) : std::nullopt;
    }

    /**
     * @brief Найти пользователя по username
     */
    std::optional<domain::User> findByUsername(const std::string& username) override {
        std::string id;
        {
            std::lock_guard<std::mutex> lock(indexMutex_);
            auto it = usernameIndex_.find(username);
            if (it == usernameIndex_.end()) {
                return std::nullopt;
            }
            id = it->second;
        }
        return findById(id);
    }

    /**
     * @brief Проверить существование username
     */
    bool existsByUsername(const std::string& username) override {
        std::lock_guard<std::mutex> lock(indexMutex_);
        return usernameIndex_.find(username) != usernameIndex_.end();
    }

    /**
     * @brief Удалить пользователя
     */
    bool deleteById(const std::string& id) override {
        auto user = users_.find(id);
        if (!user) {
            return false;
        }
        
        {
            std::lock_guard<std::mutex> lock(indexMutex_);
            usernameIndex_.erase(user->username);
        }
        
        users_.remove(id);
        return true;
    }

    /**
     * @brief Получить или создать пользователя по username
     * 
     * Удобный метод для MVP - если пользователь не существует, создаём его.
     */
    domain::User getOrCreate(const std::string& username) {
        auto existing = findByUsername(username);
        if (existing) {
            return *existing;
        }

        domain::User user(generateUuid(), username);
        save(user);
        return user;
    }

    /**
     * @brief Очистить репозиторий (для тестов)
     */
    void clear() {
        users_.clear();
        std::lock_guard<std::mutex> lock(indexMutex_);
        usernameIndex_.clear();
    }

private:
    ThreadSafeMap<std::string, domain::User> users_;
    mutable std::mutex indexMutex_;
    std::unordered_map<std::string, std::string> usernameIndex_; // username -> id
    std::mt19937_64 rng_;

    void initTestUsers() {
        // Тестовые пользователи для демо
        save(domain::User("user-001", "trader1"));
        save(domain::User("user-002", "trader2"));
        save(domain::User("user-003", "admin"));
    }

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
};

} // namespace trading::adapters::secondary
