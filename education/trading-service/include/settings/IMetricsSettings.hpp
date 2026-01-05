#pragma once

#include <string>
#include <vector>

namespace trading::settings {

/**
 * @brief Определение метрики для Prometheus
 * 
 * Содержит метаданные метрики: имя, описание и тип.
 * Используется для генерации HELP и TYPE комментариев в Prometheus формате.
 */
struct MetricDefinition {
    std::string name;   ///< Имя метрики (например, "http_requests_total")
    std::string help;   ///< Описание метрики для HELP
    std::string type;   ///< Тип метрики: "counter", "gauge", "histogram"
};

/**
 * @brief Интерфейс настроек метрик
 * 
 * Определяет контракт для конфигурации системы метрик.
 * Каждый микросервис реализует свой класс настроек с соответствующими
 * endpoints и бизнес-метриками.
 * 
 * @note Все ключи метрик должны быть определены заранее в getAllKeys(),
 *       так как ShardedCache не поддерживает итерацию.
 * 
 * @example
 * ```cpp
 * class TradingMetricsSettings : public IMetricsSettings {
 * public:
 *     std::vector<MetricDefinition> getDefinitions() const override {
 *         return {{"http_requests_total", "Total HTTP requests", "counter"}};
 *     }
 *     
 *     std::vector<std::string> getAllKeys() const override {
 *         return {"http_requests_total{method=\"GET\",path=\"/health\"}"};
 *     }
 * };
 * ```
 */
class IMetricsSettings {
public:
    virtual ~IMetricsSettings() = default;
    
    /**
     * @brief Получить определения всех метрик
     * 
     * Используется для генерации HELP и TYPE комментариев
     * в Prometheus формате.
     * 
     * @return Вектор определений метрик
     */
    virtual std::vector<MetricDefinition> getDefinitions() const = 0;
    
    /**
     * @brief Получить все ключи метрик с labels
     * 
     * Возвращает полный список всех возможных ключей метрик,
     * включая все комбинации labels. Используется для:
     * - Определения размера кэша
     * - Инициализации метрик нулями
     * - Итерации при сериализации в Prometheus формат
     * 
     * @return Вектор ключей в формате "metric_name{label1=\"value1\",label2=\"value2\"}"
     */
    virtual std::vector<std::string> getAllKeys() const = 0;
};

} // namespace trading::settings
