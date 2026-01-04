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

// Ports
#include "ports/input/IMarketService.hpp"
#include "ports/input/IOrderService.hpp"
#include "ports/input/IPortfolioService.hpp"
#include "ports/output/IBrokerGateway.hpp"
#include "ports/output/IAuthClient.hpp"
#include "ports/output/IEventPublisher.hpp"
#include "ports/output/IEventConsumer.hpp"

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

// Primary Adapters
#include "adapters/primary/HealthHandler.hpp"
#include "adapters/primary/MarketHandler.hpp"
#include "adapters/primary/OrderHandler.hpp"
#include "adapters/primary/PortfolioHandler.hpp"

#include <iostream>
#include <memory>

namespace di = boost::di;

namespace trading {

/**
 * @brief Trading Service Application (Event-Driven)
 * 
 * Публикует: order.create, order.cancel (в trading.events)
 * Слушает: order.*, quote.updated, portfolio.updated (из broker.events)
 * HTTP: GET для чтения, POST/DELETE публикуют события в RabbitMQ
 */
class TradingApp : public BoostBeastApplication {
public:
    TradingApp() { std::cout << "[TradingApp] Initializing..." << std::endl; }
    ~TradingApp() override { std::cout << "[TradingApp] Shutting down..." << std::endl; }

protected:
    void loadEnvironment(int argc, char* argv[]) override {
        BoostBeastApplication::loadEnvironment(argc, argv);
        std::cout << "[TradingApp] Environment loaded" << std::endl;
    }

    void configureInjection() override {
        std::cout << "[TradingApp] Configuring DI..." << std::endl;

        // RabbitMQ (один экземпляр для Publisher и Consumer)
        auto rabbitSettings = std::make_shared<settings::RabbitMQSettings>();
        rabbitMQAdapter_ = std::make_shared<adapters::secondary::RabbitMQAdapter>(rabbitSettings);

        auto injector = di::make_injector(
            di::bind<settings::AuthClientSettings>().to<settings::AuthClientSettings>().in(di::singleton),
            di::bind<settings::IBrokerClientSettings>().to<settings::BrokerClientSettings>().in(di::singleton),
            di::bind<settings::RabbitMQSettings>().to(rabbitSettings),
            di::bind<settings::CacheSettings>().to<settings::CacheSettings>().in(di::singleton),
            
            di::bind<IHttpClient>().to<HttpClient>().in(di::singleton),
            di::bind<adapters::secondary::HttpBrokerGateway>().to<adapters::secondary::HttpBrokerGateway>().in(di::singleton),
            di::bind<ports::output::IAuthClient>().to<adapters::secondary::HttpAuthClient>().in(di::singleton),
            di::bind<trading::ports::output::IBrokerGateway>().to<adapters::secondary::CachedBrokerGateway>().in(di::singleton),
            
            di::bind<ports::output::IEventPublisher>().to(rabbitMQAdapter_),
            di::bind<ports::output::IEventConsumer>().to(rabbitMQAdapter_),
            
            di::bind<ports::input::IMarketService>().to<application::MarketService>().in(di::singleton),
            di::bind<ports::input::IOrderService>().to<application::OrderService>().in(di::singleton),
            di::bind<ports::input::IPortfolioService>().to<application::PortfolioService>().in(di::singleton)
        );

        // HTTP Handlers
        handlers_[getHandlerKey("GET", "/health")] = injector.create<std::shared_ptr<adapters::primary::HealthHandler>>();
        
        auto marketHandler = injector.create<std::shared_ptr<adapters::primary::MarketHandler>>();
        handlers_[getHandlerKey("GET", "/api/v1/instruments")] = marketHandler;
        handlers_[getHandlerKey("GET", "/api/v1/instruments/*")] = marketHandler;
        handlers_[getHandlerKey("GET", "/api/v1/quotes")] = marketHandler;
        
        auto orderHandler = injector.create<std::shared_ptr<adapters::primary::OrderHandler>>();
        handlers_[getHandlerKey("POST", "/api/v1/orders")] = orderHandler;
        handlers_[getHandlerKey("GET", "/api/v1/orders")] = orderHandler;
        handlers_[getHandlerKey("GET", "/api/v1/orders/*")] = orderHandler;
        handlers_[getHandlerKey("DELETE", "/api/v1/orders/*")] = orderHandler;
        
        auto portfolioHandler = injector.create<std::shared_ptr<adapters::primary::PortfolioHandler>>();
        handlers_[getHandlerKey("GET", "/api/v1/portfolio")] = portfolioHandler;
        handlers_[getHandlerKey("GET", "/api/v1/portfolio/*")] = portfolioHandler;

        // Event Handler (слушает broker.events)
        auto eventConsumer = injector.create<std::shared_ptr<ports::output::IEventConsumer>>();
        tradingEventHandler_ = std::make_shared<application::TradingEventHandler>(eventConsumer);
        
        tradingEventHandler_->onOrderUpdate([](const application::TradingEventHandler::OrderUpdate& u) {
            std::cout << "[TradingApp] Order " << u.orderId << " -> " << u.status << std::endl;
        });
        
        tradingEventHandler_->onPortfolioUpdate([](const std::string& accountId, const nlohmann::json&) {
            std::cout << "[TradingApp] Portfolio updated: " << accountId << std::endl;
        });

        std::cout << "[TradingApp] Ready (events via RabbitMQ)" << std::endl;
    }

private:
    std::shared_ptr<adapters::secondary::RabbitMQAdapter> rabbitMQAdapter_;
    std::shared_ptr<application::TradingEventHandler> tradingEventHandler_;
};

} // namespace trading
