#pragma once

#include <string>
#include <map>

// TODO: namespace -> serverlib
namespace trading::ports::input {

/**
 * @brief Интерфейс сервиса метрик
 * 
 * Определяет контракт для сбора и сериализации метрик в формате Prometheus.
 * Поддерживает только counter метрики с опциональными labels.
 * 
 * @note Потокобезопасность обеспечивается реализацией (ShardedCache).
 * 
 * @example
 * ```cpp
 * // Инкремент без labels
 * metricsService->increment("orders_created_total");
 * 
 * // Инкремент с labels
 * metricsService->increment("http_requests_total", {
 *     {"method", "POST"},
 *     {"path", "/api/v1/orders"}
 * });
 * 
 * // Получить Prometheus формат
 * std::string output = metricsService->toPrometheusFormat();
 * ```
 */
class IMetricsService {
public:
    virtual ~IMetricsService() = default;
    
    /**
     * @brief Инкрементировать счётчик метрики
     * 
     * Увеличивает значение счётчика на 1. Если метрика с данным
     * ключом не существует, создаёт её со значением 1.
     * 
     * @param name Имя метрики (например, "http_requests_total")
     * @param labels Опциональные labels в формате {key: value}
     * 
     * @note Ключ метрики формируется как "name{label1=\"value1\",label2=\"value2\"}"
     */
    virtual void increment(
        const std::string& name, 
        const std::map<std::string, std::string>& labels = {}
    ) = 0;
    
    /**
     * @brief Сериализовать метрики в Prometheus формат
     * 
     * Генерирует текстовое представление всех метрик в формате,
     * понятном Prometheus:
     * ```
     * # HELP metric_name Description
     * # TYPE metric_name counter
     * metric_name{label="value"} 42
     * ```
     * 
     * @return Строка в Prometheus text format (version 0.0.4)
     */
    virtual std::string toPrometheusFormat() const = 0;
};

} // namespace trading::ports::input
