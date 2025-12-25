#pragma once

#include <IHttpHandler.hpp>
#include <atomic>
#include <sstream>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief HTTP Handler для Prometheus метрик
 * 
 * Endpoint: GET /metrics
 * 
 * Формат: Prometheus text exposition format
 */
class MetricsHandler : public IHttpHandler
{
public:
    MetricsHandler()
    {
        std::cout << "[MetricsHandler] Created" << std::endl;
    }

    void handle(IRequest& req, IResponse& res) override
    {
        ++httpRequestsTotal_;
        
        std::string metrics = buildPrometheusMetrics();
        
        res.setStatus(200);
        res.setHeader("Content-Type", "text/plain; charset=utf-8");
        res.setBody(metrics);
    }

    // Методы для сбора метрик (вызываются другими компонентами)
    void recordRequest() { ++httpRequestsTotal_; }
    void recordCacheHit() { ++cacheHits_; }
    void recordCacheMiss() { ++cacheMisses_; }
    void recordOrderPlaced() { ++ordersPlaced_; }
    void recordOrderFilled() { ++ordersFilled_; }
    void recordOrderCancelled() { ++ordersCancelled_; }

private:
    std::atomic<int64_t> httpRequestsTotal_{0};
    std::atomic<int64_t> cacheHits_{0};
    std::atomic<int64_t> cacheMisses_{0};
    std::atomic<int64_t> ordersPlaced_{0};
    std::atomic<int64_t> ordersFilled_{0};
    std::atomic<int64_t> ordersCancelled_{0};

    std::string buildPrometheusMetrics() const
    {
        std::ostringstream oss;
        
        // HTTP метрики
        oss << "# HELP trading_http_requests_total Total HTTP requests\n";
        oss << "# TYPE trading_http_requests_total counter\n";
        oss << "trading_http_requests_total " << httpRequestsTotal_.load() << "\n\n";
        
        // Cache метрики
        oss << "# HELP trading_cache_hits_total Cache hits\n";
        oss << "# TYPE trading_cache_hits_total counter\n";
        oss << "trading_cache_hits_total " << cacheHits_.load() << "\n\n";
        
        oss << "# HELP trading_cache_misses_total Cache misses\n";
        oss << "# TYPE trading_cache_misses_total counter\n";
        oss << "trading_cache_misses_total " << cacheMisses_.load() << "\n\n";
        
        // Order метрики
        oss << "# HELP trading_orders_placed_total Orders placed\n";
        oss << "# TYPE trading_orders_placed_total counter\n";
        oss << "trading_orders_placed_total " << ordersPlaced_.load() << "\n\n";
        
        oss << "# HELP trading_orders_filled_total Orders filled\n";
        oss << "# TYPE trading_orders_filled_total counter\n";
        oss << "trading_orders_filled_total " << ordersFilled_.load() << "\n\n";
        
        oss << "# HELP trading_orders_cancelled_total Orders cancelled\n";
        oss << "# TYPE trading_orders_cancelled_total counter\n";
        oss << "trading_orders_cancelled_total " << ordersCancelled_.load() << "\n\n";
        
        // Info метрика
        oss << "# HELP trading_app_info Application info\n";
        oss << "# TYPE trading_app_info gauge\n";
        oss << "trading_app_info{version=\"1.0.0\",architecture=\"hexagonal\"} 1\n";
        
        return oss.str();
    }
};

} // namespace trading::adapters::primary
