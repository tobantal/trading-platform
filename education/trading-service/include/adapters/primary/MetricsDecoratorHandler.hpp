// trading-service/include/adapters/primary/MetricsDecoratorHandler.hpp
#pragma once

#include <IHttpHandler.hpp>
#include <IRequest.hpp>
#include <IResponse.hpp>
#include "ports/input/IMetricsService.hpp"

#include <memory>
#include <iostream>
#include <regex>

namespace trading::adapters::primary
{

    /**
     * @brief Декоратор для подсчёта HTTP метрик
     *
     * Оборачивает любой IHttpHandler и инкрементирует счётчик запросов
     * перед делегированием обработки внутреннему handler'у.
     *
     * Метрика: http_requests_total{method="...",path="..."}
     */
    class MetricsDecoratorHandler : public IHttpHandler
    {
    public:
        MetricsDecoratorHandler(
            std::shared_ptr<IHttpHandler> inner,
            std::shared_ptr<ports::input::IMetricsService> metrics) : inner_(std::move(inner)), metrics_(std::move(metrics))
        {
            std::cout << "[MetricsDecoratorHandler] Created" << std::endl;
        }

        void handle(IRequest &req, IResponse &res) override
        {
            // Инкрементируем ДО обработки (считаем входящие запросы)
            metrics_->increment("http_requests_total", {{"method", req.getMethod()},
                                                        {"path", req.getPathPattern()}});

            // Делегируем обработку
            inner_->handle(req, res);
        }

    private:
        std::shared_ptr<IHttpHandler> inner_;
        std::shared_ptr<ports::input::IMetricsService> metrics_;
    };

} // namespace trading::adapters::primary