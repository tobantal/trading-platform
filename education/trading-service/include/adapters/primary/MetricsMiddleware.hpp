// trading-service/include/adapters/primary/MetricsMiddleware.hpp
#pragma once

#include <IHttpHandler.hpp>
#include <IRequest.hpp>
#include <IResponse.hpp>
#include "ports/input/IMetricsService.hpp"

#include <memory>
#include <iostream>
#include <regex>

// TODO: перенести в библиотеку cpp-http-server-lib после успешного внедрения.
namespace serverlib
{

    /**
     * @brief Middleware для подсчёта HTTP метрик
     *
     * Инкрементирует счётчик запросов.
     *
     * Метрика: http_requests_total{method="...",path="..."}
     */
    class MetricsMiddleware : public IHttpHandler
    {
    public:
        MetricsMiddleware(
            std::shared_ptr<trading::ports::input::IMetricsService> metrics) : metrics_(std::move(metrics))
        {
            std::cout << "[MetricsMiddleware] Created" << std::endl;
        }

        void handle(IRequest &req, IResponse &res) override
        {
            // Инкрементируем ДО обработки (считаем входящие запросы)
            metrics_->increment("http_requests_total", {{"method", req.getMethod()},
                                                        {"path", req.getPathPattern()}});
        }

    private:
        std::shared_ptr<trading::ports::input::IMetricsService> metrics_;
    };

} // namespace serverlib