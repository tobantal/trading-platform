#pragma once

#include "ports/input/IStrategyService.hpp"
#include "ports/output/IStrategyRepository.hpp"
#include "ports/output/IEventBus.hpp"
#include "domain/events/StrategyStartedEvent.hpp"
#include "domain/events/StrategyStoppedEvent.hpp"
#include <memory>
#include <random>
#include <sstream>
#include <iomanip>

namespace trading::application {

/**
 * @brief Сервис управления торговыми стратегиями
 * 
 * Реализует IStrategyService для создания, запуска,
 * остановки и мониторинга стратегий.
 */
class StrategyService : public ports::input::IStrategyService {
public:
    StrategyService(
        std::shared_ptr<ports::output::IStrategyRepository> strategyRepository,
        std::shared_ptr<ports::output::IEventBus> eventBus
    ) : strategyRepository_(std::move(strategyRepository))
      , eventBus_(std::move(eventBus))
      , rng_(std::random_device{}())
    {}

    /**
     * @brief Создать новую стратегию
     */
    domain::Strategy createStrategy(const domain::StrategyRequest& request) override {
        domain::Strategy strategy(
            generateUuid(),
            request.accountId,
            request.name,
            request.type,
            request.figi,
            request.config
        );

        strategyRepository_->save(strategy);
        return strategy;
    }

    /**
     * @brief Получить стратегию по ID
     */
    std::optional<domain::Strategy> getStrategyById(const std::string& strategyId) override {
        return strategyRepository_->findById(strategyId);
    }

    /**
     * @brief Запустить стратегию
     */
    bool startStrategy(const std::string& strategyId) override {
        auto strategy = strategyRepository_->findById(strategyId);
        if (!strategy) {
            return false;
        }

        if (strategy->isRunning()) {
            return false; // Уже запущена
        }

        strategyRepository_->updateStatus(strategyId, domain::StrategyStatus::RUNNING);

        // Публикуем событие
        domain::StrategyStartedEvent event;
        event.strategyId = strategyId;
        event.accountId = strategy->accountId;
        eventBus_->publish(event);

        return true;
    }

    /**
     * @brief Остановить стратегию
     */
    bool stopStrategy(const std::string& strategyId) override {
        auto strategy = strategyRepository_->findById(strategyId);
        if (!strategy) {
            return false;
        }

        if (!strategy->isRunning()) {
            return false; // Уже остановлена
        }

        strategyRepository_->updateStatus(strategyId, domain::StrategyStatus::STOPPED);

        // Публикуем событие
        domain::StrategyStoppedEvent event;
        event.strategyId = strategyId;
        event.accountId = strategy->accountId;
        event.reason = "Stopped by user";
        eventBus_->publish(event);

        return true;
    }

    /**
     * @brief Получить текущий статус стратегии
     */
    domain::StrategyStatus getStatus(const std::string& strategyId) override {
        auto strategy = strategyRepository_->findById(strategyId);
        if (!strategy) {
            return domain::StrategyStatus::STOPPED;
        }
        return strategy->status;
    }

    /**
     * @brief Получить все стратегии счёта
     */
    std::vector<domain::Strategy> getStrategiesByAccount(const std::string& accountId) override {
        return strategyRepository_->findByAccountId(accountId);
    }

    /**
     * @brief Получить последние сигналы стратегии
     */
    std::vector<domain::Signal> getRecentSignals(const std::string& strategyId, int limit) override {
        return strategyRepository_->findSignalsByStrategyId(strategyId, limit);
    }

    /**
     * @brief Удалить стратегию
     */
    bool deleteStrategy(const std::string& strategyId) override {
        auto strategy = strategyRepository_->findById(strategyId);
        if (!strategy) {
            return false;
        }

        // Нельзя удалить запущенную стратегию
        if (strategy->isRunning()) {
            return false;
        }

        return strategyRepository_->deleteById(strategyId);
    }

    /**
     * @brief Обновить конфигурацию стратегии
     */
    bool updateConfig(const std::string& strategyId, const std::string& config) override {
        auto strategy = strategyRepository_->findById(strategyId);
        if (!strategy) {
            return false;
        }

        // Нельзя обновить запущенную стратегию
        if (strategy->isRunning()) {
            return false;
        }

        strategyRepository_->updateConfig(strategyId, config);
        return true;
    }

private:
    std::shared_ptr<ports::output::IStrategyRepository> strategyRepository_;
    std::shared_ptr<ports::output::IEventBus> eventBus_;
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
