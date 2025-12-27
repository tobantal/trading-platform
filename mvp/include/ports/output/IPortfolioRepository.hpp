#pragma once

#include "domain/Portfolio.hpp"
#include <string>
#include <optional>

namespace trading::ports::output {

/**
 * @brief Интерфейс репозитория портфелей
 * 
 * Output Port для сохранения и загрузки портфелей из БД.
 */
class IPortfolioRepository {
public:
    virtual ~IPortfolioRepository() = default;

    /**
     * @brief Сохранить портфель
     * 
     * @param portfolio Портфель для сохранения
     */
    virtual void save(const domain::Portfolio& portfolio) = 0;

    /**
     * @brief Найти портфель по ID счёта
     * 
     * @param accountId UUID счёта
     * @return Portfolio или nullopt
     */
    virtual std::optional<domain::Portfolio> findByAccountId(const std::string& accountId) = 0;

    /**
     * @brief Удалить портфель по ID счёта
     * 
     * @param accountId UUID счёта
     * @return true если удалён
     */
    virtual bool deleteByAccountId(const std::string& accountId) = 0;

    /**
     * @brief Обновить денежные средства портфеля
     * 
     * @param accountId UUID счёта
     * @param cash Новое значение денежных средств
     */
    virtual void updateCash(const std::string& accountId, const domain::Money& cash) = 0;

    /**
     * @brief Обновить позицию в портфеле
     * 
     * @param accountId UUID счёта
     * @param position Позиция для обновления
     */
    virtual void updatePosition(const std::string& accountId, const domain::Position& position) = 0;

    /**
     * @brief Удалить позицию из портфеля
     * 
     * @param accountId UUID счёта
     * @param figi FIGI инструмента
     * @return true если удалена
     */
    virtual bool deletePosition(const std::string& accountId, const std::string& figi) = 0;

    /**
     * @brief Получить все портфели пользователя
     * 
     * @param userId UUID пользователя
     * @return Список портфелей
     */
    virtual std::vector<domain::Portfolio> findByUserId(const std::string& userId) = 0;
};

} // namespace trading::ports::output