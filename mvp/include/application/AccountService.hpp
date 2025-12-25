#pragma once

#include "ports/input/IAccountService.hpp"
#include "ports/output/IAccountRepository.hpp"
#include <memory>
#include <random>
#include <sstream>
#include <iomanip>

namespace trading::application {

/**
 * @brief Сервис управления брокерскими счетами
 * 
 * Реализует IAccountService для создания, получения
 * и управления счетами пользователей.
 */
class AccountService : public ports::input::IAccountService {
public:
    explicit AccountService(
        std::shared_ptr<ports::output::IAccountRepository> accountRepository
    ) : accountRepository_(std::move(accountRepository))
      , rng_(std::random_device{}())
    {}

    /**
     * @brief Добавить новый брокерский счёт
     */
    domain::Account addAccount(
        const std::string& userId,
        const domain::AccountRequest& request
    ) override {
        domain::Account account(
            generateUuid(),
            userId,
            request.name,
            request.type,
            request.accessToken,
            true // isActive
        );

        accountRepository_->save(account);
        return account;
    }

    /**
     * @brief Получить список счетов пользователя
     */
    std::vector<domain::Account> getUserAccounts(const std::string& userId) override {
        return accountRepository_->findByUserId(userId);
    }

    /**
     * @brief Получить счёт по ID
     */
    std::optional<domain::Account> getAccountById(const std::string& accountId) override {
        return accountRepository_->findById(accountId);
    }

    /**
     * @brief Установить активный счёт для пользователя
     */
    bool setActiveAccount(const std::string& userId, const std::string& accountId) override {
        auto account = accountRepository_->findById(accountId);
        if (!account || account->userId != userId) {
            return false;
        }
        
        accountRepository_->setActiveAccount(userId, accountId);
        return true;
    }

    /**
     * @brief Получить активный счёт пользователя
     */
    std::optional<domain::Account> getActiveAccount(const std::string& userId) override {
        return accountRepository_->findActiveByUserId(userId);
    }

    /**
     * @brief Удалить счёт
     */
    bool deleteAccount(const std::string& accountId) override {
        return accountRepository_->deleteById(accountId);
    }

private:
    std::shared_ptr<ports::output::IAccountRepository> accountRepository_;
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
};

} // namespace trading::application
