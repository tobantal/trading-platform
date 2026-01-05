#pragma once

#include "IMetricsSettings.hpp"
#include <vector>
#include <string>

namespace trading::settings {

/**
 * @brief Настройки метрик для Trading Service
 * 
 * Определяет все метрики и их ключи для Trading Service:
 * - HTTP метрики (запросы к endpoints)
 * - Event метрики (события из RabbitMQ)
 * - Бизнес метрики (ордера)
 * 
 * @note Все endpoints и события должны быть перечислены явно,
 *       так как ShardedCache не поддерживает динамическое добавление
 *       ключей с последующей итерацией.
 */
class MetricsSettings : public IMetricsSettings {
public:
    /**
     * @brief Получить определения метрик
     * 
     * @return Вектор определений с HELP и TYPE для каждой метрики
     */
    std::vector<MetricDefinition> getDefinitions() const override {
        return {
            {"http_requests_total", "Total HTTP requests", "counter"},
            {"events_received_total", "Total events received from RabbitMQ", "counter"},
            {"orders_created_total", "Total orders created", "counter"},
            {"orders_filled_total", "Total orders filled", "counter"},
            {"orders_rejected_total", "Total orders rejected", "counter"},
            {"orders_cancelled_total", "Total orders cancelled", "counter"}
        };
    }
    
    /**
     * @brief Получить все ключи метрик
     * 
     * Полный список всех метрик с labels для Trading Service.
     * Размер этого списка определяет capacity кэша.
     * 
     * @return Вектор ключей в Prometheus формате
     */
    std::vector<std::string> getAllKeys() const override {
        return {
            // ============================================
            // HTTP метрики (method + path)
            // ============================================
            
            // Health & Metrics
            "http_requests_total{method=\"GET\",path=\"/health\"}",
            "http_requests_total{method=\"GET\",path=\"/metrics\"}",
            
            // Portfolio
            "http_requests_total{method=\"GET\",path=\"/api/v1/portfolio\"}",
            
            // Orders
            "http_requests_total{method=\"GET\",path=\"/api/v1/orders\"}",
            "http_requests_total{method=\"POST\",path=\"/api/v1/orders\"}",
            "http_requests_total{method=\"DELETE\",path=\"/api/v1/orders\"}",
            
            // Market data
            "http_requests_total{method=\"GET\",path=\"/api/v1/instruments\"}",
            "http_requests_total{method=\"GET\",path=\"/api/v1/quotes\"}",
            
            // ============================================
            // Event метрики (routing key)
            // ============================================
            "events_received_total{event=\"order.created\"}",
            "events_received_total{event=\"order.filled\"}",
            "events_received_total{event=\"order.rejected\"}",
            "events_received_total{event=\"order.cancelled\"}",
            "events_received_total{event=\"portfolio.updated\"}",
            
            // ============================================
            // Бизнес метрики (без labels)
            // ============================================
            "orders_created_total",
            "orders_filled_total",
            "orders_rejected_total",
            "orders_cancelled_total"
        };
    }
};

} // namespace trading::settings
