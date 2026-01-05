#pragma once

#include "ports/input/IMetricsService.hpp"
#include "settings/IMetricsSettings.hpp"

#include <memory>
#include <string>
#include <sstream>
#include <map>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <iostream>

namespace trading::application {

/**
 * @brief Сервис сбора и хранения метрик
 *
 * Реализация IMetricsService с использованием unordered_map и atomic
 * для потокобезопасного хранения счётчиков.
 *
 * Особенности:
 * - Потокобезопасность через shared_mutex (read) / unique_lock (write)
 * - Атомарные счётчики для lock-free инкремента
 * - Метрики не вытесняются (в отличие от LRU кэша)
 */
class MetricsService : public ports::input::IMetricsService {
public:
    explicit MetricsService(std::shared_ptr<settings::IMetricsSettings> settings)
        : settings_(std::move(settings))
    {
        std::cout << "[MetricsService] Initializing..." << std::endl;
        
        // Инициализируем все метрики нулями
        for (const auto& key : settings_->getAllKeys()) {
            counters_[key] = std::make_unique<std::atomic<int64_t>>(0);
        }
        
        std::cout << "[MetricsService] Initialized with " 
                  << counters_.size() << " metrics" << std::endl;
    }
    
    void increment(
        const std::string& name,
        const std::map<std::string, std::string>& labels = {}
    ) override {
        std::string key = buildKey(name, labels);
        
        // Fast path: ключ уже существует
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = counters_.find(key);
            if (it != counters_.end()) {
                it->second->fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }
        
        // Slow path: создаём новый ключ
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            // Double-check после получения exclusive lock
            auto it = counters_.find(key);
            if (it != counters_.end()) {
                it->second->fetch_add(1, std::memory_order_relaxed);
            } else {
                counters_[key] = std::make_unique<std::atomic<int64_t>>(1);
            }
        }
    }
    
    std::string toPrometheusFormat() const override {
        std::ostringstream oss;
        
        // HELP и TYPE
        for (const auto& def : settings_->getDefinitions()) {
            oss << "# HELP " << def.name << " " << def.help << "\n";
            oss << "# TYPE " << def.name << " " << def.type << "\n";
        }
        
        // Значения
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            for (const auto& key : settings_->getAllKeys()) {
                auto it = counters_.find(key);
                if (it != counters_.end()) {
                    oss << key << " " << it->second->load(std::memory_order_relaxed) << "\n";
                }
            }
        }
        
        return oss.str();
    }

private:
    std::shared_ptr<settings::IMetricsSettings> settings_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<std::atomic<int64_t>>> counters_;
    
    std::string buildKey(
        const std::string& name,
        const std::map<std::string, std::string>& labels
    ) const {
        if (labels.empty()) {
            return name;
        }
        
        std::ostringstream oss;
        oss << name << "{";
        bool first = true;
        for (const auto& [k, v] : labels) {
            if (!first) oss << ",";
            oss << k << "=\"" << v << "\"";
            first = false;
        }
        oss << "}";
        return oss.str();
    }
};

} // namespace trading::application
