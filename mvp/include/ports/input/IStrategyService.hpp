#pragma once

#include "domain/StrategyRequest.hpp"
#include "domain/enums/StrategyStatus.hpp"
#include "domain/Strategy.hpp"
#include "domain/Signal.hpp"
#include <string>
#include <vector>
#include <optional>

namespace trading::ports::input {

/**
 * @brief Интерфейс сервиса управления торговыми стратегиями
 * 
 * Input Port для создания, запуска/остановки и мониторинга стратегий.
 */
class IStrategyService {
public:
    virtual ~IStrategyService() = default;

    /**
     * @brief Создать новую стратегию
     * 
     * @param request Параметры стратегии
     * @return Созданная Strategy
     */
    virtual domain::Strategy createStrategy(const domain::StrategyRequest& request) = 0;

    /**
     * @brief Получить стратегию по ID
     * 
     * @param strategyId ID стратегии
     * @return Strategy или nullopt
     */
    virtual std::optional<domain::Strategy> getStrategyById(const std::string& strategyId) = 0;

    /**
     * @brief Запустить стратегию
     * 
     * @param strategyId ID стратегии
     * @return true если запуск успешен
     * 
     * @note Публикует StrategyStartedEvent при успехе
     */
    virtual bool startStrategy(const std::string& strategyId) = 0;

    /**
     * @brief Остановить стратегию
     * 
     * @param strategyId ID стратегии
     * @return true если остановка успешна
     * 
     * @note Публикует StrategyStoppedEvent при успехе
     */
    virtual bool stopStrategy(const std::string& strategyId) = 0;

    /**
     * @brief Получить текущий статус стратегии
     * 
     * @param strategyId ID стратегии
     * @return Статус или STOPPED если не найдена
     */
    virtual domain::StrategyStatus getStatus(const std::string& strategyId) = 0;

    /**
     * @brief Получить список стратегий пользователя/счёта
     * 
     * @param accountId ID счёта
     * @return Список стратегий
     */
    virtual std::vector<domain::Strategy> getStrategiesByAccount(const std::string& accountId) = 0;

    /**
     * @brief Получить последние сигналы стратегии
     * 
     * @param strategyId ID стратегии
     * @param limit Максимальное количество
     * @return Список сигналов (новые первые)
     */
    virtual std::vector<domain::Signal> getRecentSignals(
        const std::string& strategyId,
        int limit = 10
    ) = 0;

    /**
     * @brief Удалить стратегию
     * 
     * @param strategyId ID стратегии
     * @return true если удаление успешно
     * 
     * @note Стратегия должна быть остановлена
     */
    virtual bool deleteStrategy(const std::string& strategyId) = 0;

    /**
     * @brief Обновить конфигурацию стратегии
     * 
     * @param strategyId ID стратегии
     * @param config Новая конфигурация (JSON)
     * @return true если обновление успешно
     * 
     * @note Стратегия должна быть остановлена
     */
    virtual bool updateConfig(const std::string& strategyId, const std::string& config) = 0;
};

} // namespace trading::ports::input