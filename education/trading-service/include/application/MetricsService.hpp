#pragma once

#include "ports/input/IMetricsService.hpp"
#include "settings/IMetricsSettings.hpp"

#include <cache/concurrency/ShardedCache.hpp>
#include <cache/Cache.hpp>
#include <cache/eviction/LRUPolicy.hpp>

#include <memory>
#include <string>
#include <sstream>
#include <map>
#include <iostream>

namespace trading::application {

/**
 * @brief Сервис сбора и хранения метрик
 * 
 * Реализация IMetricsService с использованием ShardedCache для
 * потокобезопасного хранения счётчиков. Поддерживает только counter метрики.
 * 
 * Особенности:
 * - Потокобезопасность через ShardedCache (16 шардов)
 * - Все ключи метрик инициализируются нулями при создании
 * - Без mutex — ShardedCache обеспечивает конкурентный доступ
 * 
 * @note Размер кэша определяется количеством ключей в settings->getAllKeys()
 * 
 * @example
 * ```cpp
 * auto settings = std::make_shared<MetricsSettings>();
 * auto metricsService = std::make_shared<MetricsService>(settings);
 * 
 * metricsService->increment("http_requests_total", {{"method", "GET"}, {"path", "/health"}});
 * 
 * std::string output = metricsService->toPrometheusFormat();
 * ```
 */
class MetricsService : public ports::input::IMetricsService {
public:
    /**
     * @brief Конструктор
     * 
     * Инициализирует ShardedCache с размером, равным количеству ключей
     * в settings, и заполняет все метрики нулевыми значениями.
     * 
     * @param settings Настройки метрик с определениями и ключами
     */
    explicit MetricsService(std::shared_ptr<settings::IMetricsSettings> settings)
        : settings_(std::move(settings))
    {
        std::cout << "[MetricsService] Initializing..." << std::endl;
        
        size_t capacity = settings_->getAllKeys().size();
        
        counters_ = std::make_unique<ShardedCache<std::string, int64_t, 16>>(
            capacity, 
            [](size_t cap) {
                return std::make_unique<Cache<std::string, int64_t>>(
                    cap, 
                    std::make_unique<LRUPolicy<std::string>>()
                );
            }
        );
        
        // Инициализируем все метрики нулями
        for (const auto& key : settings_->getAllKeys()) {
            counters_->put(key, 0);
        }
        
        std::cout << "[MetricsService] Initialized with " << capacity << " metrics" << std::endl;
    }
    
    /**
     * @brief Инкрементировать счётчик
     * 
     * @param name Имя метрики
     * @param labels Labels в формате {key: value}
     */
    void increment(
        const std::string& name, 
        const std::map<std::string, std::string>& labels = {}
    ) override {
        std::string key = buildKey(name, labels);
        
        auto current = counters_->get(key);
        int64_t newValue = current ? (*current + 1) : 1;
        counters_->put(key, newValue);
    }
    
    /**
     * @brief Сериализовать в Prometheus формат
     * 
     * @return Строка с метриками в text format 0.0.4
     */
    std::string toPrometheusFormat() const override {
        std::ostringstream oss;
        
        // HELP и TYPE для каждой метрики
        for (const auto& def : settings_->getDefinitions()) {
            oss << "# HELP " << def.name << " " << def.help << "\n";
            oss << "# TYPE " << def.name << " " << def.type << "\n";
        }
        oss << "\n";
        
        // Значения — итерируем по известным ключам из settings
        for (const auto& key : settings_->getAllKeys()) {
            auto value = counters_->get(key);
            if (value) {
                oss << key << " " << *value << "\n";
            }
        }
        
        return oss.str();
    }

private:
    std::shared_ptr<settings::IMetricsSettings> settings_;
    std::unique_ptr<ShardedCache<std::string, int64_t, 16>> counters_;
    
    /**
     * @brief Построить ключ метрики с labels
     * 
     * @param name Имя метрики
     * @param labels Labels
     * @return Ключ в формате "name{l1=\"v1\",l2=\"v2\"}" или просто "name"
     */
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
