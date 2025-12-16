#pragma once

#include "domain/Strategy.hpp"
#include "domain/Signal.hpp"
#include <string>
#include <optional>
#include <vector>

namespace trading::ports::output {

/**
 * @brief Интерфейс репозитория стратегий
 * 
 * Output Port для сохранения и загрузки стратегий из БД.
 */
class IStrategyRepository {
public:
    virtual ~IStrategyRepository() = default;

    // ============================================
    // STRATEGIES
    // ============================================

    /**
     * @brief Сохранить стратегию
     * 
     * @param strategy Стратегия для сохранения
     */
    virtual void save(const domain::Strategy& strategy) = 0;

    /**
     * @brief Найти стратегию по ID
     * 
     * @param id UUID стратегии
     * @return Strategy или nullopt
     */
    virtual std::optional<domain::Strategy> findById(const std::string& id) = 0;

    /**
     * @brief Найти все стратегии счёта
     * 
     * @param accountId UUID счёта
     * @return Список стратегий
     */
    virtual std::vector<domain::Strategy> findByAccountId(const std::string& accountId) = 0;

    /**
     * @brief Найти стратегии по статусу
     * 
     * @param status Статус стратегии
     * @return Список стратегий
     */
    virtual std::vector<domain::Strategy> findByStatus(domain::StrategyStatus status) = 0;

    /**
     * @brief Обновить статус стратегии
     * 
     * @param strategyId UUID стратегии
     * @param status Новый статус
     */
    virtual void updateStatus(const std::string& strategyId, domain::StrategyStatus status) = 0;

    /**
     * @brief Обновить конфигурацию стратегии
     * 
     * @param strategyId UUID стратегии
     * @param config Новая конфигурация (JSON)
     */
    virtual void updateConfig(const std::string& strategyId, const std::string& config) = 0;

    /**
     * @brief Удалить стратегию
     * 
     * @param id UUID стратегии
     * @return true если удалена
     */
    virtual bool deleteById(const std::string& id) = 0;

    // ============================================
    // SIGNALS
    // ============================================

    /**
     * @brief Сохранить сигнал
     * 
     * @param signal Сигнал для сохранения
     */
    virtual void saveSignal(const domain::Signal& signal) = 0;

    /**
     * @brief Найти сигналы стратегии
     * 
     * @param strategyId UUID стратегии
     * @param limit Максимальное количество
     * @return Список сигналов (новые первые)
     */
    virtual std::vector<domain::Signal> findSignalsByStrategyId(
        const std::string& strategyId,
        int limit = 100
    ) = 0;

    /**
     * @brief Удалить все сигналы стратегии
     * 
     * @param strategyId UUID стратегии
     */
    virtual void deleteSignalsByStrategyId(const std::string& strategyId) = 0;
};

} // namespace trading::ports::output