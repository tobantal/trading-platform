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

// Ports
#include "ports/input/IMarketService.hpp"
#include "ports/input/IOrderService.hpp"
#include "ports/input/IPortfolioService.hpp"
#include "ports/output/IBrokerGateway.hpp"
#include "ports/output/IAuthClient.hpp"
#include "ports/output/IEventPublisher.hpp"
#include "ports/output/IEventConsumer.hpp"
#include "ports/output/IIdempotencyRepository.hpp"

// Application
#include "application/MarketService.hpp"
#include "application/OrderService.hpp"
#include "application/PortfolioService.hpp"
#include "application/TradingEventHandler.hpp"

// Secondary Adapters
#include "adapters/secondary/HttpBrokerGateway.hpp"
#include "adapters/secondary/CachedBrokerGateway.hpp"
#include "adapters/secondary/HttpAuthClient.hpp"
#include "adapters/secondary/events/RabbitMQAdapter.hpp"
#include "adapters/secondary/PostgresIdempotencyRepository.hpp"

// Primary Adapters
#include "adapters/primary/HealthHandler.hpp"
#include "adapters/primary/MarketHandler.hpp"
#include "adapters/primary/OrderHandler.hpp"
#include "adapters/primary/PortfolioHandler.hpp"
#include "adapters/primary/IdempotentHandler.hpp"

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

        void configureInjection() override
        {
            std::cout << "[TradingApp] Configuring DI..." << std::endl;

            // Шаг 1: Создаём RabbitMQAdapter через DI (один экземпляр для Publisher и Consumer)
            auto rabbitInjector = di::make_injector(
                di::bind<settings::RabbitMQSettings>().in(di::singleton));
            auto rabbitMQAdapter = rabbitInjector.create<std::shared_ptr<adapters::secondary::RabbitMQAdapter>>();

            // Шаг 2: Основной injector с instance binding для RabbitMQ
            auto injector = di::make_injector(
                di::bind<settings::DbSettings>().in(di::singleton),
                di::bind<settings::AuthClientSettings>().in(di::singleton),
                di::bind<settings::IBrokerClientSettings>().to<settings::BrokerClientSettings>().in(di::singleton),
                di::bind<settings::RabbitMQSettings>().in(di::singleton),
                di::bind<settings::CacheSettings>().in(di::singleton),

                di::bind<trading::ports::output::IIdempotencyRepository>()
                    .to<trading::adapters::secondary::PostgresIdempotencyRepository>()
                    .in(di::singleton),

                di::bind<IHttpClient>().to<HttpClient>().in(di::singleton),
                di::bind<adapters::secondary::HttpBrokerGateway>().in(di::singleton),
                di::bind<ports::output::IAuthClient>().to<adapters::secondary::HttpAuthClient>().in(di::singleton),
                di::bind<trading::ports::output::IBrokerGateway>().to<adapters::secondary::CachedBrokerGateway>().in(di::singleton),

                // RabbitMQ - один экземпляр для обоих интерфейсов
                di::bind<ports::output::IEventPublisher>().to(rabbitMQAdapter),
                di::bind<ports::output::IEventConsumer>().to(rabbitMQAdapter),

                di::bind<ports::input::IMarketService>().to<application::MarketService>().in(di::singleton),
                di::bind<ports::input::IOrderService>().to<application::OrderService>().in(di::singleton),
                di::bind<ports::input::IPortfolioService>().to<application::PortfolioService>().in(di::singleton));

            // Шаг 3: HTTP Handlers
            handlers_[getHandlerKey("GET", "/health")] = injector.create<std::shared_ptr<adapters::primary::HealthHandler>>();

            auto marketHandler = injector.create<std::shared_ptr<adapters::primary::MarketHandler>>();
            handlers_[getHandlerKey("GET", "/api/v1/instruments")] = marketHandler;
            handlers_[getHandlerKey("GET", "/api/v1/instruments/*")] = marketHandler;
            handlers_[getHandlerKey("GET", "/api/v1/quotes")] = marketHandler;

            auto orderHandler = injector.create<std::shared_ptr<adapters::primary::OrderHandler>>();

            // Декоратор для идемпотентности
            auto idempotencyRepo = injector.create<std::shared_ptr<trading::ports::output::IIdempotencyRepository>>();
            auto idempotentOrderHandler = std::make_shared<adapters::primary::IdempotentHandler>(
                orderHandler, idempotencyRepo);

            handlers_[getHandlerKey("POST", "/api/v1/orders")] = idempotentOrderHandler;
            handlers_[getHandlerKey("GET", "/api/v1/orders")] = idempotentOrderHandler;
            handlers_[getHandlerKey("GET", "/api/v1/orders/*")] = idempotentOrderHandler;
            handlers_[getHandlerKey("DELETE", "/api/v1/orders/*")] = idempotentOrderHandler;

            auto portfolioHandler = injector.create<std::shared_ptr<adapters::primary::PortfolioHandler>>();
            handlers_[getHandlerKey("GET", "/api/v1/portfolio")] = portfolioHandler;
            handlers_[getHandlerKey("GET", "/api/v1/portfolio/*")] = portfolioHandler;

            // Шаг 4: Event Handler через DI (слушает broker.events)
            // TradingEventHandler вызывает subscribe() в конструкторе
            tradingEventHandler_ = injector.create<std::shared_ptr<application::TradingEventHandler>>();

            tradingEventHandler_->onOrderUpdate([](const application::TradingEventHandler::OrderUpdate &u)
                                                { std::cout << "[TradingApp] Order " << u.orderId << " -> " << u.status << std::endl; });

            tradingEventHandler_->onPortfolioUpdate([](const std::string &accountId, const nlohmann::json &)
                                                    { std::cout << "[TradingApp] Portfolio updated: " << accountId << std::endl; });

            // Шаг 5: Запускаем RabbitMQ ПОСЛЕ регистрации всех handlers
            std::cout << "[TradingApp] Starting RabbitMQ..." << std::endl;
            rabbitMQAdapter->start();

            std::cout << "[TradingApp] Ready (events via RabbitMQ)" << std::endl;
        }

    private:
        std::shared_ptr<application::TradingEventHandler> tradingEventHandler_;
    };

} // namespace trading
