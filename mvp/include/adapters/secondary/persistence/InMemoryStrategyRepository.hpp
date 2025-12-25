#pragma once

#include "ports/output/IStrategyRepository.hpp"
#include <ThreadSafeMap.hpp>
#include <mutex>
#include <algorithm>
#include <set>

namespace trading::adapters::secondary {

/**
 * @brief In-memory реализация репозитория торговых стратегий
 */
class InMemoryStrategyRepository : public ports::output::IStrategyRepository {
public:
    /**
     * @brief Сохранить стратегию
     */
    void save(const domain::Strategy& strategy) override {
        strategies_.insert(strategy.id, std::make_shared<domain::Strategy>(strategy));
        
        std::lock_guard<std::mutex> lock(indexMutex_);
        accountStrategies_[strategy.accountId].insert(strategy.id);
    }

    /**
     * @brief Найти стратегию по ID
     */
    std::optional<domain::Strategy> findById(const std::string& id) override {
        auto strategy = strategies_.find(id);
        return strategy ? std::optional(*strategy) : std::nullopt;
    }

    /**
     * @brief Найти все стратегии счёта
     */
    std::vector<domain::Strategy> findByAccountId(const std::string& accountId) override {
        std::vector<domain::Strategy> result;
        
        std::set<std::string> strategyIds;
        {
            std::lock_guard<std::mutex> lock(indexMutex_);
            auto it = accountStrategies_.find(accountId);
            if (it != accountStrategies_.end()) {
                strategyIds = it->second;
            }
        }
        
        for (const auto& id : strategyIds) {
            if (auto strategy = strategies_.find(id)) {
                result.push_back(*strategy);
            }
        }
        
        return result;
    }

    /**
     * @brief Найти стратегии по статусу
     */
    std::vector<domain::Strategy> findByStatus(domain::StrategyStatus status) override {
        std::vector<domain::Strategy> result;
        auto all = strategies_.getAll();
        
        for (const auto& strategy : all) {
            if (strategy->status == status) {
                result.push_back(*strategy);
            }
        }
        
        return result;
    }

    /**
     * @brief Обновить статус стратегии
     */
    void updateStatus(const std::string& strategyId, domain::StrategyStatus status) override {
        auto strategy = strategies_.find(strategyId);
        if (strategy) {
            auto updated = std::make_shared<domain::Strategy>(*strategy);
            if (status == domain::StrategyStatus::RUNNING) {
                updated->start();
            } else if (status == domain::StrategyStatus::STOPPED) {
                updated->stop();
            } else if (status == domain::StrategyStatus::ERROR) {
                updated->setError("Status set to ERROR");
            }
            strategies_.insert(strategyId, updated);
        }
    }

    /**
     * @brief Обновить конфигурацию стратегии
     */
    void updateConfig(const std::string& strategyId, const std::string& config) override {
        auto strategy = strategies_.find(strategyId);
        if (strategy) {
            auto updated = std::make_shared<domain::Strategy>(*strategy);
            updated->config = config;
            updated->updatedAt = domain::Timestamp::now();
            strategies_.insert(strategyId, updated);
        }
    }

    /**
     * @brief Удалить стратегию
     */
    bool deleteById(const std::string& id) override {
        auto strategy = strategies_.find(id);
        if (!strategy) {
            return false;
        }
        
        {
            std::lock_guard<std::mutex> lock(indexMutex_);
            accountStrategies_[strategy->accountId].erase(id);
        }
        
        // Удаляем связанные сигналы
        deleteSignalsByStrategyId(id);
        
        strategies_.remove(id);
        return true;
    }

    /**
     * @brief Сохранить сигнал
     */
    void saveSignal(const domain::Signal& signal) override {
        signals_.insert(signal.id, std::make_shared<domain::Signal>(signal));
        
        std::lock_guard<std::mutex> lock(indexMutex_);
        strategySignals_[signal.strategyId].push_back(signal.id);
    }

    /**
     * @brief Найти сигналы стратегии
     */
    std::vector<domain::Signal> findSignalsByStrategyId(
        const std::string& strategyId,
        int limit
    ) override {
        std::vector<domain::Signal> result;
        
        std::vector<std::string> signalIds;
        {
            std::lock_guard<std::mutex> lock(indexMutex_);
            auto it = strategySignals_.find(strategyId);
            if (it != strategySignals_.end()) {
                signalIds = it->second;
            }
        }
        
        // Берём последние limit сигналов
        size_t start = signalIds.size() > static_cast<size_t>(limit) ? 
                       signalIds.size() - static_cast<size_t>(limit) : 0;
        for (size_t i = start; i < signalIds.size(); ++i) {
            if (auto signal = signals_.find(signalIds[i])) {
                result.push_back(*signal);
            }
        }
        
        // Сортируем по времени (новые первые)
        std::sort(result.begin(), result.end(),
            [](const domain::Signal& a, const domain::Signal& b) {
                return a.createdAt > b.createdAt;
            });
        
        return result;
    }

    /**
     * @brief Удалить все сигналы стратегии
     */
    void deleteSignalsByStrategyId(const std::string& strategyId) override {
        std::vector<std::string> signalIds;
        {
            std::lock_guard<std::mutex> lock(indexMutex_);
            auto it = strategySignals_.find(strategyId);
            if (it != strategySignals_.end()) {
                signalIds = it->second;
                strategySignals_.erase(it);
            }
        }
        
        for (const auto& id : signalIds) {
            signals_.remove(id);
        }
    }

    /**
     * @brief Очистить репозиторий
     */
    void clear() {
        strategies_.clear();
        signals_.clear();
        std::lock_guard<std::mutex> lock(indexMutex_);
        accountStrategies_.clear();
        strategySignals_.clear();
    }

private:
    ThreadSafeMap<std::string, domain::Strategy> strategies_;
    ThreadSafeMap<std::string, domain::Signal> signals_;
    mutable std::mutex indexMutex_;
    std::unordered_map<std::string, std::set<std::string>> accountStrategies_;
    std::unordered_map<std::string, std::vector<std::string>> strategySignals_;
};

} // namespace trading::adapters::secondary
