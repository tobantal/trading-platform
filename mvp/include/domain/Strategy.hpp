#pragma once

#include "enums/StrategyType.hpp"
#include "enums/StrategyStatus.hpp"
#include "Timestamp.hpp"
#include <string>
#include <cstdint>

namespace trading::domain {

    /**
 * @brief Торговая стратегия
 * 
 * Описывает автоматическую стратегию торговли.
 * Для MVP поддерживается только SMA Crossover.
 */
struct Strategy {
    std::string id;             ///< UUID стратегии
    std::string accountId;      ///< FK на accounts
    std::string name;           ///< Название ("SBER SMA Crossover")
    StrategyType type;          ///< Тип стратегии
    std::string figi;           ///< FIGI инструмента
    std::string config;         ///< JSON с параметрами
    StrategyStatus status;      ///< Текущий статус
    std::string errorMessage;   ///< Сообщение об ошибке (если ERROR)
    Timestamp createdAt;        ///< Дата создания
    Timestamp updatedAt;        ///< Дата последнего обновления

    Strategy() : status(StrategyStatus::STOPPED) {}

    Strategy(
        const std::string& id,
        const std::string& accountId,
        const std::string& name,
        StrategyType type,
        const std::string& figi,
        const std::string& config
    ) : id(id), accountId(accountId), name(name), type(type),
        figi(figi), config(config), status(StrategyStatus::STOPPED),
        createdAt(Timestamp::now()), updatedAt(Timestamp::now()) {}

    /**
     * @brief Запустить стратегию
     */
    bool start() {
        if (status == StrategyStatus::RUNNING) {
            return false;
        }
        status = StrategyStatus::RUNNING;
        errorMessage.clear();
        updatedAt = Timestamp::now();
        return true;
    }

    /**
     * @brief Остановить стратегию
     */
    bool stop() {
        if (status == StrategyStatus::STOPPED) {
            return false;
        }
        status = StrategyStatus::STOPPED;
        updatedAt = Timestamp::now();
        return true;
    }

    /**
     * @brief Установить статус ошибки
     */
    void setError(const std::string& message) {
        status = StrategyStatus::ERROR;
        errorMessage = message;
        updatedAt = Timestamp::now();
    }

    /**
     * @brief Проверить, запущена ли стратегия
     */
    bool isRunning() const {
        return status == StrategyStatus::RUNNING;
    }
};

} // namespace trading::domain