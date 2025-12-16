#pragma once

#include "domain/Order.hpp"
#include "domain/Timestamp.hpp"
#include <string>
#include <optional>
#include <vector>

namespace trading::ports::output {

/**
 * @brief Интерфейс репозитория ордеров
 * 
 * Output Port для сохранения и загрузки ордеров из БД.
 */
class IOrderRepository {
public:
    virtual ~IOrderRepository() = default;

    /**
     * @brief Сохранить ордер
     * 
     * @param order Ордер для сохранения
     */
    virtual void save(const domain::Order& order) = 0;

    /**
     * @brief Найти ордер по ID
     * 
     * @param id UUID ордера
     * @return Order или nullopt
     */
    virtual std::optional<domain::Order> findById(const std::string& id) = 0;

    /**
     * @brief Найти все ордера счёта
     * 
     * @param accountId UUID счёта
     * @return Список ордеров
     */
    virtual std::vector<domain::Order> findByAccountId(const std::string& accountId) = 0;

    /**
     * @brief Найти ордера по статусу
     * 
     * @param accountId UUID счёта
     * @param status Статус ордера
     * @return Список ордеров
     */
    virtual std::vector<domain::Order> findByStatus(
        const std::string& accountId,
        domain::OrderStatus status
    ) = 0;

    /**
     * @brief Найти ордера за период
     * 
     * @param accountId UUID счёта
     * @param from Начало периода
     * @param to Конец периода
     * @return Список ордеров
     */
    virtual std::vector<domain::Order> findByPeriod(
        const std::string& accountId,
        const domain::Timestamp& from,
        const domain::Timestamp& to
    ) = 0;

    /**
     * @brief Обновить статус ордера
     * 
     * @param orderId UUID ордера
     * @param status Новый статус
     */
    virtual void updateStatus(const std::string& orderId, domain::OrderStatus status) = 0;

    /**
     * @brief Удалить ордер
     * 
     * @param id UUID ордера
     * @return true если удалён
     */
    virtual bool deleteById(const std::string& id) = 0;
};

} // namespace trading::ports::output