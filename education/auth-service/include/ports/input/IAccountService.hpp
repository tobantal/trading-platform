#pragma once

#include "domain/Account.hpp"
#include "domain/enums/AccountType.hpp"
#include <string>
#include <optional>
#include <vector>

namespace auth::ports::input {

/**
 * @brief Запрос на создание аккаунта
 */
struct CreateAccountRequest {
    std::string name;
    domain::AccountType type;
    std::string tinkoffToken;  // Будет зашифрован
};

/**
 * @brief Интерфейс сервиса управления аккаунтами
 */
class IAccountService {
public:
    virtual ~IAccountService() = default;

    /**
     * @brief Создать новый аккаунт
     */
    virtual domain::Account createAccount(
        const std::string& userId,
        const CreateAccountRequest& request
    ) = 0;

    /**
     * @brief Получить аккаунты пользователя
     */
    virtual std::vector<domain::Account> getUserAccounts(const std::string& userId) = 0;

    /**
     * @brief Получить аккаунт по ID
     */
    virtual std::optional<domain::Account> getAccountById(const std::string& accountId) = 0;

    /**
     * @brief Удалить аккаунт
     */
    virtual bool deleteAccount(const std::string& accountId) = 0;

    /**
     * @brief Проверить принадлежит ли аккаунт пользователю
     */
    virtual bool isAccountOwner(const std::string& userId, const std::string& accountId) = 0;
};

} // namespace auth::ports::input
