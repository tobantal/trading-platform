#pragma once

#include "domain/AccountRequest.hpp"
#include "domain/Account.hpp"
#include <string>
#include <vector>
#include <optional>

namespace trading::ports::input {

/**
 * @brief Интерфейс сервиса управления брокерскими счетами
 * 
 * Input Port для работы со счетами пользователя.
 * Каждый пользователь может иметь несколько счетов (sandbox, production).
 */
class IAccountService {
public:
    virtual ~IAccountService() = default;

    /**
     * @brief Добавить новый брокерский счёт
     * 
     * @param userId ID пользователя
     * @param request Данные для создания счёта
     * @return Созданный Account
     */
    virtual domain::Account addAccount(
        const std::string& userId,
        const domain::AccountRequest& request
    ) = 0;

    /**
     * @brief Получить список счетов пользователя
     * 
     * @param userId ID пользователя
     * @return Список счетов
     */
    virtual std::vector<domain::Account> getUserAccounts(const std::string& userId) = 0;

    /**
     * @brief Получить счёт по ID
     * 
     * @param accountId ID счёта
     * @return Account или nullopt
     */
    virtual std::optional<domain::Account> getAccountById(const std::string& accountId) = 0;

    /**
     * @brief Удалить счёт
     * 
     * @param accountId ID счёта
     * @return true если успешно
     */
    virtual bool deleteAccount(const std::string& accountId) = 0;
};

} // namespace trading::ports::input