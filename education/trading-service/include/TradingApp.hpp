// include/TradingApp.hpp
#pragma once

#include <BoostBeastApplication.hpp>
#include <HttpClient.hpp>
#include <boost/di.hpp>

// Settings
#include "settings/AuthClientSettings.hpp"
#include "settings/BrokerClientSettings.hpp"
#include "settings/RabbitMQSettings.hpp"
#include "settings/CacheSettings.hpp"
#include "settings/DbSettings.hpp"
#include "settings/IMetricsSettings.hpp"
#include "settings/MetricsSettings.hpp"

// Ports
#include "ports/input/IMarketService.hpp"
#include "ports/input/IOrderService.hpp"
#include "ports/input/IPortfolioService.hpp"
#include "ports/output/IBrokerGateway.hpp"
#include "ports/output/IAuthClient.hpp"
#include "ports/output/IEventPublisher.hpp"
#include "ports/input/IEventConsumer.hpp"
#include "ports/output/IIdempotencyRepository.hpp"

// Application
#include "application/MarketService.hpp"
#include "application/OrderService.hpp"
#include "application/PortfolioService.hpp"
#include "application/TradingEventHandler.hpp"
#include "application/MetricsService.hpp"

// Secondary Adapters
#include "adapters/secondary/HttpBrokerGateway.hpp"
#include "adapters/secondary/CachedBrokerGateway.hpp"
#include "adapters/secondary/HttpAuthClient.hpp"
#include "adapters/secondary/events/RabbitMQAdapter.hpp"
#include "adapters/secondary/PostgresIdempotencyRepository.hpp"

// Primary Adapters
#include "adapters/primary/HealthHandler.hpp"
#include "adapters/primary/MetricsHandler.hpp"

#include "adapters/primary/AllEventsListener.hpp"

#include "adapters/primary/GetQuotesHandler.hpp"
#include "adapters/primary/GetAllInstrumentsHandler.hpp"
#include "adapters/primary/SearchInstrumentsHandler.hpp"
#include "adapters/primary/GetInstrumentByFigiHandler.hpp"

#include "adapters/primary/GetPortfolioHandler.hpp"
#include "adapters/primary/GetPositionsHandler.hpp"
#include "adapters/primary/GetCashHandler.hpp"

#include "adapters/primary/CreateOrderHandler.hpp"
#include "adapters/primary/GetOrdersHandler.hpp"
#include "adapters/primary/GetOrderHandler.hpp"
#include "adapters/primary/CancelOrderHandler.hpp"

// Primary Adapters Middleware
#include "adapters/primary/ChainHandler.hpp"
#include "adapters/primary/MetricsMiddleware.hpp"
#include "adapters/primary/IdempotencyCacheReader.hpp"
#include "adapters/primary/IdempotencyCacheWriter.hpp"
#include "adapters/primary/AccountIdExtractorMiddleware.hpp"

#include <iostream>
#include <memory>

namespace di = boost::di;

namespace trading
{

        /**
         * @brief Trading Service Application (Event-Driven)
         *
         * Публикует: order.create, order.cancel (в trading.events)
         * Слушает: order.*, quote.updated, portfolio.updated (из broker.events)
         * HTTP: GET для чтения, POST/DELETE публикуют события в RabbitMQ
         */
        class TradingApp : public BoostBeastApplication
        {
        public:
                TradingApp() { std::cout << "[TradingApp] Initializing..." << std::endl; }
                ~TradingApp() override { std::cout << "[TradingApp] Shutting down..." << std::endl; }

        protected:
                void loadEnvironment(int argc, char *argv[]) override
                {
                        BoostBeastApplication::loadEnvironment(argc, argv);
                        std::cout << "[TradingApp] Environment loaded" << std::endl;
                }

                // TODO: метод потом переместить в библиотеку cpp-http-server-lib
                template <typename... Handlers>
                void registerEndpoint(const std::string &method, const std::string &path, Handlers &&...handlers)
                {
                        handlers_[getHandlerKey(method, path)] =
                            std::make_shared<serverlib::ChainHandler>(std::forward<Handlers>(handlers)...);
                }

                void configureInjection() override
                {
                        std::cout << "[TradingApp] Configuring DI..." << std::endl;

                        // Шаг 1: Создаём RabbitMQAdapter через DI
                        auto rabbitInjector = di::make_injector(
                            di::bind<settings::RabbitMQSettings>().in(di::singleton));
                        auto rabbitMQAdapter = rabbitInjector.create<std::shared_ptr<adapters::secondary::RabbitMQAdapter>>();

                        // Шаг 2: Основной injector
                        auto injector = di::make_injector(
                            // Settings
                            di::bind<settings::DbSettings>().in(di::singleton),
                            di::bind<settings::AuthClientSettings>().in(di::singleton),
                            di::bind<settings::IBrokerClientSettings>().to<settings::BrokerClientSettings>().in(di::singleton),
                            di::bind<settings::RabbitMQSettings>().in(di::singleton),
                            di::bind<settings::CacheSettings>().in(di::singleton),
                            di::bind<settings::IMetricsSettings>().to<settings::MetricsSettings>().in(di::singleton),

                            // Repositories
                            di::bind<ports::output::IIdempotencyRepository>()
                                .to<adapters::secondary::PostgresIdempotencyRepository>()
                                .in(di::singleton),

                            // Clients
                            di::bind<IHttpClient>().to<HttpClient>().in(di::singleton),
                            di::bind<adapters::secondary::HttpBrokerGateway>().in(di::singleton),
                            di::bind<ports::output::IAuthClient>().to<adapters::secondary::HttpAuthClient>().in(di::singleton),
                            di::bind<ports::output::IBrokerGateway>().to<adapters::secondary::CachedBrokerGateway>().in(di::singleton),

                            // RabbitMQ
                            di::bind<ports::output::IEventPublisher>().to(rabbitMQAdapter),
                            di::bind<ports::input::IEventConsumer>().to(rabbitMQAdapter),

                            // Services
                            di::bind<ports::input::IMetricsService>().to<application::MetricsService>().in(di::singleton),
                            di::bind<ports::input::IMarketService>().to<application::MarketService>().in(di::singleton),
                            di::bind<ports::input::IOrderService>().to<application::OrderService>().in(di::singleton),
                            di::bind<ports::input::IPortfolioService>().to<application::PortfolioService>().in(di::singleton));

                        // Middleware
                        auto metricsMiddleware = injector.create<std::shared_ptr<serverlib::MetricsMiddleware>>();
                        auto idempotencyCacheReader = injector.create<std::shared_ptr<adapters::primary::IdempotencyCacheReader>>();
                        auto idempotencyCacheWriter = injector.create<std::shared_ptr<adapters::primary::IdempotencyCacheWriter>>();
                        auto accountIdExtractorMiddleware = injector.create<std::shared_ptr<adapters::primary::AccountIdExtractorMiddleware>>();

                        // Шаг 3: Получаем MetricsService для декораторов
                        auto metricsService = injector.create<std::shared_ptr<ports::input::IMetricsService>>();

                        // Шаг 4: HTTP Handlers

                        // Health (с метриками)
                        auto healthHandler = injector.create<std::shared_ptr<adapters::primary::HealthHandler>>();
                        registerEndpoint("GET", "/health",
                                         metricsMiddleware, healthHandler);

                        // Metrics (без middleware — сам себя не считает)
                        registerEndpoint("GET", "/metrics",
                                         injector.create<std::shared_ptr<adapters::primary::MetricsHandler>>());

                        // Market (с метриками)
                        auto getQuotesHandler = injector.create<std::shared_ptr<adapters::primary::GetQuotesHandler>>();
                        auto getAllInstrumentsHandler = injector.create<std::shared_ptr<adapters::primary::GetAllInstrumentsHandler>>();
                        auto searchInstrumentsHandler = injector.create<std::shared_ptr<adapters::primary::SearchInstrumentsHandler>>();
                        auto getInstrumentByFigiHandler = injector.create<std::shared_ptr<adapters::primary::GetInstrumentByFigiHandler>>();

                        registerEndpoint("GET", "/api/v1/quotes",
                                         metricsMiddleware, getQuotesHandler);
                        registerEndpoint("GET", "/api/v1/instruments",
                                         metricsMiddleware, getAllInstrumentsHandler);
                        registerEndpoint("GET", "/api/v1/instruments/search",
                                         metricsMiddleware, searchInstrumentsHandler);
                        registerEndpoint("GET", "/api/v1/instruments/*",
                                         metricsMiddleware, getInstrumentByFigiHandler);

                        // Orders (с идемпотентностью и метриками)
                        auto createOrderHandler = injector.create<std::shared_ptr<adapters::primary::CreateOrderHandler>>();
                        auto getOrdersHandler = injector.create<std::shared_ptr<adapters::primary::GetOrdersHandler>>();
                        auto getOrderHandler = injector.create<std::shared_ptr<adapters::primary::GetOrderHandler>>();
                        auto cancelOrderHandler = injector.create<std::shared_ptr<adapters::primary::CancelOrderHandler>>();

                        registerEndpoint("GET", "/api/v1/orders",
                                         metricsMiddleware, accountIdExtractorMiddleware, idempotencyCacheReader, getOrdersHandler, idempotencyCacheWriter);
                        registerEndpoint("GET", "/api/v1/orders/*",
                                         metricsMiddleware, accountIdExtractorMiddleware, idempotencyCacheReader, getOrderHandler, idempotencyCacheWriter);
                        registerEndpoint("DELETE", "/api/v1/orders/*",
                                         metricsMiddleware, accountIdExtractorMiddleware, idempotencyCacheReader, cancelOrderHandler, idempotencyCacheWriter);
                        registerEndpoint("POST", "/api/v1/orders",
                                         metricsMiddleware, accountIdExtractorMiddleware, idempotencyCacheReader, createOrderHandler, idempotencyCacheWriter);

                        // Portfolio (с метриками и accountId middleware)
                        auto getPortfolioHandler = injector.create<std::shared_ptr<adapters::primary::GetPortfolioHandler>>();
                        auto getPositionsHandler = injector.create<std::shared_ptr<adapters::primary::GetPositionsHandler>>();
                        auto getCashHandler = injector.create<std::shared_ptr<adapters::primary::GetCashHandler>>();

                        registerEndpoint("GET", "/api/v1/portfolio",
                                         metricsMiddleware, accountIdExtractorMiddleware, getPortfolioHandler);
                        registerEndpoint("GET", "/api/v1/portfolio/positions",
                                         metricsMiddleware, accountIdExtractorMiddleware, getPositionsHandler);
                        registerEndpoint("GET", "/api/v1/portfolio/cash",
                                         metricsMiddleware, accountIdExtractorMiddleware, getCashHandler);

                        // Шаг 5: Event Handlers
                        auto tradingEventHandler = injector.create<std::shared_ptr<application::TradingEventHandler>>();
                        tradingEventHandler->onOrderUpdate([](const application::TradingEventHandler::OrderUpdate &u)
                                                           { std::cout << "[TradingApp] Order " << u.orderId << " -> " << u.status << std::endl; });
                        tradingEventHandler->onPortfolioUpdate([](const std::string &accountId, const nlohmann::json &)
                                                               { std::cout << "[TradingApp] Portfolio updated: " << accountId << std::endl; });

                        // AllEventsListener для метрик
                        auto allEventsListener = std::make_shared<adapters::primary::AllEventsListener>(rabbitMQAdapter, metricsService);

                        // Шаг 6: Запускаем RabbitMQ
                        std::cout << "[TradingApp] Starting RabbitMQ..." << std::endl;
                        rabbitMQAdapter->start();

                        std::cout << "[TradingApp] Ready (events via RabbitMQ)" << std::endl;
                }
        };

} // namespace trading
