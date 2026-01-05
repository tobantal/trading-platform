#pragma once

#include <IHttpHandler.hpp>
#include <IRequest.hpp>
#include <IResponse.hpp>
#include "ports/input/IMetricsService.hpp"

#include <memory>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief HTTP handler для endpoint /metrics
 * 
 * Возвращает метрики в формате Prometheus text format 0.0.4.
 * Prometheus периодически опрашивает этот endpoint для сбора метрик.
 * 
 * @note Content-Type: text/plain; version=0.0.4; charset=utf-8
 * 
 * @example Response:
 * ```
 * # HELP http_requests_total Total HTTP requests
 * # TYPE http_requests_total counter
 * http_requests_total{method="GET",path="/health"} 42
 * ```
 */
class MetricsHandler : public IHttpHandler {
public:
    /**
     * @brief Конструктор
     * 
     * @param metrics Сервис метрик для получения данных
     */
    explicit MetricsHandler(std::shared_ptr<ports::input::IMetricsService> metrics)
        : metrics_(std::move(metrics))
    {
        std::cout << "[MetricsHandler] Created" << std::endl;
    }
    
    /**
     * @brief Обработать запрос GET /metrics
     * 
     * @param req HTTP запрос (не используется)
     * @param res HTTP ответ с метриками в Prometheus формате
     */
    void handle(IRequest& req, IResponse& res) override {
        res.setStatus(200);
        res.setHeader("Content-Type", "text/plain; version=0.0.4; charset=utf-8");
        res.setBody(metrics_->toPrometheusFormat());
    }

private:
    std::shared_ptr<ports::input::IMetricsService> metrics_;
};

} // namespace trading::adapters::primary
