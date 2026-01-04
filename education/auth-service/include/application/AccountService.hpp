#pragma once

#include "ports/input/IAccountService.hpp"
#include "ports/output/IAccountRepository.hpp"
#include <memory>
#include <iostream>

namespace auth::application {

/**
 * @brief Сервис управления брокерскими аккаунтами
 * 
 * ВАЖНО: Для sandbox аккаунтов генерируется ID с "sandbox" в имени:
 *   acc-sandbox-{uuid}
 * 
 * Это позволяет broker-service автоматически создавать такие аккаунты
 * при первом обращении (безопасно, т.к. sandbox — тестовые деньги).
 */
class AccountService : public ports::input::IAccountService {
public:
    explicit AccountService(
        std::shared_ptr<ports::output::IAccountRepository> accountRepo
    ) : accountRepo_(std::move(accountRepo))
    {
        std::cout << "[AccountService] Created" << std::endl;
    }

    domain::Account createAccount(
        const std::string& userId,
        const ports::input::CreateAccountRequest& request
    ) override {
        // Генерируем ID с учётом типа аккаунта
        // Для SANDBOX включаем "sandbox" в ID, чтобы broker мог автоматически создать
        std::string accountId = "acc-";
        if (request.type == domain::AccountType::SANDBOX) {
            accountId += "sandbox-";
        }
        accountId += generateUuid();
        
        // Шифруем токен (TODO: RSA в production)
        std::string encryptedToken = encodeToken(request.tinkoffToken);
        
        domain::Account account(
            accountId,
            userId,
            request.name,
            request.type,
            encryptedToken
        );
        
        std::cout << "[AccountService] Creating account: " << accountId 
                  << " type=" << domain::toString(request.type) << std::endl;
        
        return accountRepo_->save(account);
    }

    std::vector<domain::Account> getUserAccounts(const std::string& userId) override {
        return accountRepo_->findByUserId(userId);
    }

    std::optional<domain::Account> getAccountById(const std::string& accountId) override {
        return accountRepo_->findById(accountId);
    }

    bool deleteAccount(const std::string& accountId) override {
        return accountRepo_->deleteById(accountId);
    }

    bool isAccountOwner(const std::string& userId, const std::string& accountId) override {
        auto accountOpt = accountRepo_->findById(accountId);
        if (!accountOpt) {
            return false;
        }
        return accountOpt->userId == userId;
    }

private:
    std::shared_ptr<ports::output::IAccountRepository> accountRepo_;

    std::string generateUuid() {
        static int counter = 0;
        return std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) 
               + "-" + std::to_string(++counter);
    }

    std::string encodeToken(const std::string& token) {
        // TODO: Production - RSA шифрование с публичным ключом Broker
        // Для учебного проекта - base64
        if (token.empty()) return "";
        return "enc:" + token;  // Простая маркировка для демо
    }
};

} // namespace auth::application
