// include/TradingApp.hpp
#pragma once

#include <BoostBeastApplication.hpp>
#include <boost/di.hpp>

// Ports - Input
#include "ports/input/IMarketService.hpp"
#include "ports/input/IOrderService.hpp"
#include "ports/input/IPortfolioService.hpp"

// Ports - Output
#include "ports/output/IAuthClient.hpp"
#include "ports/output/IBrokerGateway.hpp"
#include "ports/output/IEventPublisher.hpp"
#include "ports/output/IEventConsumer.hpp"

// Settings
#include "settings/AuthClientSettings.hpp"
#include "settings/RabbitMQSettings.hpp"
#include "settings/CacheSettings.hpp"
#include "settings/IBrokerClientSettings.hpp"
#include "settings/BrokerClientSettings.hpp"

// Application Services
#include "application/MarketService.hpp"
#include "application/OrderService.hpp"
#include "application/PortfolioService.hpp"
#include "application/OrderEventHandler.hpp"

// Secondary Adapters
#include "adapters/secondary/HttpAuthClient.hpp"
#include "adapters/secondary/HttpBrokerGateway.hpp"
#include "adapters/secondary/CachedBrokerGateway.hpp"
#include "adapters/secondary/events/RabbitMQAdapter.hpp"
#include <HttpClient.hpp>

// Primary Adapters (Handlers)
#include "adapters/primary/HealthHandler.hpp"
#include "adapters/primary/MarketHandler.hpp"
#include "adapters/primary/OrderHandler.hpp"
#include "adapters/primary/PortfolioHandler.hpp"

#include <iostream>
#include <memory>

namespace di = boost::di;

namespace trading {

/**
 * @brief Trading Service Application
 * 
 * Hexagonal architecture with Boost.DI for dependency injection.
 * 
 * Архитектура:
 * - Публичный API с авторизацией через auth-service
 * - Проксирование к broker-service для рыночных данных
 * - RabbitMQ для команд создания/отмены ордеров
 * - Подписка на события order.created, order.rejected
 */
class TradingApp : public BoostBeastApplication {
public:
    TradingApp() {
        std::cout << "[TradingApp] Initializing..." << std::endl;
    }
    
    ~TradingApp() override {
        // Останавливаем RabbitMQ при завершении
        if (rabbitAdapter_) {
            rabbitAdapter_->stop();
        }
    }

protected:
    void loadEnvironment(int argc, char* argv[]) override {
        BoostBeastApplication::loadEnvironment(argc, argv);
        std::cout << "[TradingApp] Environment loaded" << std::endl;
    }

    void configureInjection() override {
        std::cout << "[TradingApp] Configuring Boost.DI injection..." << std::endl;

        auto injector = di::make_injector(
            // ================================================================
            // Layer 1: Settings
            // ================================================================
            di::bind<settings::AuthClientSettings>()
                .to<settings::AuthClientSettings>()
                .in(di::singleton),

            di::bind<settings::IBrokerClientSettings>()
                .to<settings::BrokerClientSettings>()
                .in(di::singleton),

            di::bind<settings::RabbitMQSettings>()
                .to<settings::RabbitMQSettings>()
                .in(di::singleton),

            di::bind<settings::CacheSettings>()
                .to<settings::CacheSettings>()
                .in(di::singleton),

            // ================================================================
            // Layer 2: Secondary Adapters (Output Ports implementations)
            // ================================================================

            // HTTP Client
            di::bind<IHttpClient>()
                .to<HttpClient>()
                .in(di::singleton),

            // HttpBrokerGateway
           di::bind<adapters::secondary::HttpBrokerGateway>()
                .to<adapters::secondary::HttpBrokerGateway>()
                .in(di::singleton),

            // Auth Client → auth-service
            di::bind<ports::output::IAuthClient>()
                .to<adapters::secondary::HttpAuthClient>()
                .in(di::singleton),

            // Cached Gateway (декоратор над HttpBrokerGateway)
            // HttpBrokerGateway создаётся автоматически как зависимость
            di::bind<trading::ports::output::IBrokerGateway>()\
                .to<adapters::secondary::CachedBrokerGateway>()
                .in(di::singleton),
                

            // RabbitMQ Adapter (Publisher + Consumer)
            di::bind<ports::output::IEventPublisher>()
                .to<adapters::secondary::RabbitMQAdapter>()
                .in(di::singleton),

            // ================================================================
            // Layer 3: Application Services
            // ================================================================
            di::bind<ports::input::IMarketService>()
                .to<application::MarketService>()
                .in(di::singleton),

            di::bind<ports::input::IOrderService>()
                .to<application::OrderService>()
                .in(di::singleton),

            di::bind<ports::input::IPortfolioService>()
                .to<application::PortfolioService>()
                .in(di::singleton)
        );

        std::cout << "[TradingApp] DI Injector configured" << std::endl;

        // ================================================================
        // Layer 4: HTTP Handlers (Primary Adapters)
        // ================================================================
        std::cout << "[TradingApp] Registering HTTP Handlers..." << std::endl;

        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::HealthHandler>>();
            handlers_[getHandlerKey("GET", "/health")] = handler;
            std::cout << "  + HealthHandler: GET /health" << std::endl;
        }

        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::MarketHandler>>();
            handlers_[getHandlerKey("GET", "/api/v1/instruments")] = handler;
            handlers_[getHandlerKey("GET", "/api/v1/instruments/*")] = handler;
            handlers_[getHandlerKey("GET", "/api/v1/quotes")] = handler;
            std::cout << "  + MarketHandler: GET /api/v1/instruments[/*], /api/v1/quotes" << std::endl;
        }

        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::OrderHandler>>();
            handlers_[getHandlerKey("POST", "/api/v1/orders")] = handler;
            handlers_[getHandlerKey("GET", "/api/v1/orders")] = handler;
            handlers_[getHandlerKey("GET", "/api/v1/orders/*")] = handler;
            handlers_[getHandlerKey("DELETE", "/api/v1/orders/*")] = handler;
            std::cout << "  + OrderHandler: POST/GET/DELETE /api/v1/orders[/*]" << std::endl;
        }

        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::PortfolioHandler>>();
            handlers_[getHandlerKey("GET", "/api/v1/portfolio")] = handler;
            handlers_[getHandlerKey("GET", "/api/v1/portfolio/*")] = handler;
            std::cout << "  + PortfolioHandler: GET /api/v1/portfolio[/*]" << std::endl;
        }

        std::cout << "[TradingApp] HTTP Handlers registered" << std::endl;

        // ================================================================
        // Layer 5: Event Subscriptions (RabbitMQ)
        // ================================================================
        std::cout << "[TradingApp] Setting up RabbitMQ subscriptions..." << std::endl;

        rabbitAdapter_ = injector.create<std::shared_ptr<adapters::secondary::RabbitMQAdapter>>();
        auto orderEventHandler = injector.create<std::shared_ptr<application::OrderEventHandler>>();

        // Подписываемся на события от broker-service
        rabbitAdapter_->subscribe(
            {"order.created", "order.rejected", "order.filled", "order.cancelled"},
            [orderEventHandler](const std::string& routingKey, const std::string& message) {
                orderEventHandler->handle(routingKey, message);
            }
        );

        // Запускаем RabbitMQ
        rabbitAdapter_->start();
        std::cout << "[TradingApp] RabbitMQ started" << std::endl;
    }

private:
    std::shared_ptr<adapters::secondary::RabbitMQAdapter> rabbitAdapter_;
};

} // namespace trading
