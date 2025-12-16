#pragma once

#include "IHttpHandler.hpp"
#include <atomic>
#include <string>
#include <cstdint>

/**
 * @class MetricsHandler
 * @brief Обработчик для Prometheus метрик
 * 
 * Endpoint: GET /metrics
 * 
 * Response (200 OK):
 * # HELP http_requests_total Total HTTP requests
 * # TYPE http_requests_total counter
 * http_requests_total 42
 * 
 * # HELP cache_hits_total Cache hits
 * # TYPE cache_hits_total counter
 * cache_hits_total 10
 * cache_misses_total 2
 */
class MetricsHandler : public IHttpHandler
{
public:
    MetricsHandler();
    virtual ~MetricsHandler() = default;

    void handle(IRequest& req, IResponse& res) override;

    // Методы для сбора метрик (вызываются другими компонентами)
    void recordRequest(const std::string& method, const std::string& path);
    void recordCacheHit();
    void recordCacheMiss();

private:
    std::atomic<int64_t> httpRequestsTotal_{0};   ///< Счётчик всех запросов
    std::atomic<int64_t> cacheHits_{0};           ///< Счётчик попаданий в кэш
    std::atomic<int64_t> cacheMisses_{0};         ///< Счётчик промахов кэша

    /**
     * @brief Построить ответ в формате Prometheus
     */
    std::string buildPrometheusMetrics() const;
};