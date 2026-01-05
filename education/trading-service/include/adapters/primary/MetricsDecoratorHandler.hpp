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
     *
     * ВАЖНО: Path нормализуется для группировки метрик:
     * - /api/v1/orders/ord-xxx -> /api/v1/orders
     * - /api/v1/instruments/BBG123 -> /api/v1/instruments
     * - /api/v1/portfolio/acc-xxx -> /api/v1/portfolio
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
            // Нормализуем path для метрик (убираем конкретные ID)
            std::string normalizedPath = normalizePath(req.getPath());

            // Инкрементируем ДО обработки (считаем входящие запросы)
            metrics_->increment("http_requests_total", {{"method", req.getMethod()},
                                                        {"path", normalizedPath}});

            // Делегируем обработку
            inner_->handle(req, res);
        }

    private:
        std::shared_ptr<IHttpHandler> inner_;
        std::shared_ptr<ports::input::IMetricsService> metrics_;

        /**
         * @brief Нормализует path для группировки метрик
         *
         * Убирает конкретные ID из path, чтобы метрики группировались:
         * - /api/v1/orders/ord-abc123 -> /api/v1/orders
         * - /api/v1/instruments/BBG004730N88 -> /api/v1/instruments
         * - /api/v1/portfolio/acc-sandbox-123 -> /api/v1/portfolio
         * - /health -> /health (без изменений)
         * - /metrics -> /metrics (без изменений)
         *
         * @param path Оригинальный path запроса
         * @return Нормализованный path для метрик
         */
        std::string normalizePath(const std::string &path) const
        {
            // Сначала убираем query string
            std::string cleanPath = path;
            size_t queryPos = cleanPath.find('?');
            if (queryPos != std::string::npos)
            {
                cleanPath = cleanPath.substr(0, queryPos);
            }

            // Паттерны для нормализации (базовый путь -> регулярка для match)
            // Порядок важен - более специфичные паттерны первыми

            // /api/v1/orders/ord-xxx -> /api/v1/orders
            if (cleanPath.find("/api/v1/orders/") == 0)
            {
                return "/api/v1/orders";
            }

            // /api/v1/instruments/xxx -> /api/v1/instruments
            if (cleanPath.find("/api/v1/instruments/") == 0)
            {
                return "/api/v1/instruments";
            }

            // /api/v1/portfolio/xxx -> /api/v1/portfolio
            if (cleanPath.find("/api/v1/portfolio/") == 0)
            {
                return "/api/v1/portfolio";
            }

            // /api/v1/quotes/xxx -> /api/v1/quotes
            if (cleanPath.find("/api/v1/quotes/") == 0)
            {
                return "/api/v1/quotes";
            }

            // Остальные пути без изменений
            return cleanPath;
        }
    };

} // namespace trading::adapters::primary