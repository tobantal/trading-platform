#pragma once

#include <IHttpHandler.hpp>
#include <IRequest.hpp>
#include <IResponse.hpp>
#include "ports/input/IMetricsService.hpp"

#include <memory>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief Декоратор для подсчёта HTTP метрик
 * 
 * Оборачивает любой IHttpHandler и инкрементирует счётчик запросов
 * перед делегированием обработки внутреннему handler'у.
 * 
 * Метрика: http_requests_total{method="...",path="..."}
 * 
 * @note Паттерн Decorator — не изменяет поведение inner handler,
 *       только добавляет сбор метрик.
 * 
 * @example
 * ```cpp
 * auto orderHandler = std::make_shared<OrderHandler>(...);
 * auto wrapped = std::make_shared<MetricsDecoratorHandler>(orderHandler, metricsService);
 * 
 * // Теперь каждый запрос к wrapped будет учитываться в метриках
 * handlers_[getHandlerKey("POST", "/api/v1/orders")] = wrapped;
 * ```
 */
class MetricsDecoratorHandler : public IHttpHandler {
public:
    /**
     * @brief Конструктор
     * 
     * @param inner Внутренний handler для делегирования обработки
     * @param metrics Сервис метрик для инкремента счётчиков
     */
    MetricsDecoratorHandler(
        std::shared_ptr<IHttpHandler> inner,
        std::shared_ptr<ports::input::IMetricsService> metrics
    ) : inner_(std::move(inner))
      , metrics_(std::move(metrics))
    {
        std::cout << "[MetricsDecoratorHandler] Created" << std::endl;
    }
    
    /**
     * @brief Обработать HTTP запрос с подсчётом метрик
     * 
     * 1. Инкрементирует http_requests_total с labels method и path
     * 2. Делегирует обработку inner handler'у
     * 
     * @param req HTTP запрос
     * @param res HTTP ответ
     */
    void handle(IRequest& req, IResponse& res) override {
        // Инкрементируем ДО обработки (считаем входящие запросы)
        metrics_->increment("http_requests_total", {
            {"method", req.getMethod()},
            {"path", req.getPath()}
        });
        
        // Делегируем обработку
        inner_->handle(req, res);
    }

private:
    std::shared_ptr<IHttpHandler> inner_;
    std::shared_ptr<ports::input::IMetricsService> metrics_;
};

} // namespace trading::adapters::primary
