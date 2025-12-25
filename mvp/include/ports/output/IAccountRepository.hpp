#pragma once

#include "domain/Account.hpp"
#include <string>
#include <optional>
#include <vector>

namespace trading::ports::output {

/**
 * @brief Интерфейс репозитория брокерских счетов
 * 
 * Output Port для сохранения и загрузки счетов из БД.
 */
class IAccountRepository {
public:
    virtual ~IAccountRepository() = default;

    /**
     * @brief Сохранить счёт
     * 
     * @param account Счёт для сохранения
     */
    virtual void save(const domain::Account& account) = 0;

    /**
     * @brief Найти счёт по ID
     * 
     * @param id UUID счёта
     * @return Account или nullopt
     */
    virtual std::optional<domain::Account> findById(const std::string& id) = 0;

    /**
     * @brief Найти все счета пользователя
     * 
     * @param userId UUID пользователя
     * @return Список счетов
     */
    virtual std::vector<domain::Account> findByUserId(const std::string& userId) = 0;

    /**
     * @brief Обновить счёт
     * 
     * @param account Счёт с обновлёнными данными
     */
    virtual void update(const domain::Account& account) = 0;

    /**
     * @brief Удалить счёт
     * 
     * @param id UUID счёта
     * @return true если удалён
     */
    virtual bool deleteById(const std::string& id) = 0;

    /**
     * @brief Установить активный счёт для пользователя
     * 
     * @param userId UUID пользователя
     * @param accountId UUID счёта для активации
     * 
     * @note Деактивирует все остальные счета пользователя
     */
    virtual void setActiveAccount(const std::string& userId, const std::string& accountId) = 0;

    /**
     * @brief Найти активный счёт пользователя
     * 
     * @param userId UUID пользователя
     * @return Активный Account или nullopt
     */
    virtual std::optional<trading::domain::Account> findActiveByUserId(const std::string& userId) = 0;
};

} // namespace trading::ports::output