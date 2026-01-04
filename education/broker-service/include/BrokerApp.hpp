// include/BrokerApp.hpp
#pragma once

#include <BoostBeastApplication.hpp>
#include <boost/di.hpp>

// Ports
#include "ports/input/IQuoteService.hpp"
#include "ports/output/IInstrumentRepository.hpp"
#include "ports/output/IQuoteRepository.hpp"
#include "ports/output/IBrokerOrderRepository.hpp"
#include "ports/output/IBrokerPositionRepository.hpp"
#include "ports/output/IBrokerBalanceRepository.hpp"
#include "ports/output/IEventPublisher.hpp"
#include "ports/output/IEventConsumer.hpp"
#include "ports/output/IBrokerGateway.hpp"

// Settings
#include "settings/DbSettings.hpp"
#include "settings/RabbitMQSettings.hpp"
#include "settings/BrokerSettings.hpp"

// Application
#include "application/QuoteService.hpp"
#include "application/OrderCommandHandler.hpp"
#include "application/MarketDataPublisher.hpp"

// Secondary Adapters
#include "adapters/secondary/PostgresInstrumentRepository.hpp"
#include "adapters/secondary/PostgresQuoteRepository.hpp"
#include "adapters/secondary/PostgresBrokerOrderRepository.hpp"
#include "adapters/secondary/PostgresBrokerPositionRepository.hpp"
#include "adapters/secondary/PostgresBrokerBalanceRepository.hpp"
#include "adapters/secondary/broker/EnhancedFakeBroker.hpp"
#include "adapters/secondary/broker/FakeBrokerAdapter.hpp"
#include "adapters/secondary/events/RabbitMQAdapter.hpp"

// Primary Adapters
#include "adapters/primary/HealthHandler.hpp"
#include "adapters/primary/MetricsHandler.hpp"
#include "adapters/primary/InstrumentsHandler.hpp"
#include "adapters/primary/QuotesHandler.hpp"
#include "adapters/primary/PortfolioHandler.hpp"
#include "adapters/primary/OrdersHandler.hpp"

#include <iostream>
#include <memory>

namespace di = boost::di;

namespace broker {

/**
 * @brief Broker Service Application (Event-Driven)
 * 
 * Слушает: order.create, order.cancel (из trading.events)
 * Публикует: order.*, quote.updated, portfolio.updated (в broker.events)
 * HTTP: только GET запросы (POST/DELETE через RabbitMQ)
 */
class BrokerApp : public BoostBeastApplication {
public:
    BrokerApp() { std::cout << "[BrokerApp] Initializing..." << std::endl; }
    ~BrokerApp() override { std::cout << "[BrokerApp] Shutting down..." << std::endl; }

protected:
    void loadEnvironment(int argc, char* argv[]) override {
        BoostBeastApplication::loadEnvironment(argc, argv);
        std::cout << "[BrokerApp] Environment loaded" << std::endl;
    }

    void configureInjection() override {
        std::cout << "[BrokerApp] Configuring DI..." << std::endl;

        // RabbitMQ (один экземпляр для Publisher и Consumer)
        auto rabbitSettings = std::make_shared<settings::RabbitMQSettings>();
        rabbitMQAdapter_ = std::make_shared<adapters::secondary::RabbitMQAdapter>(rabbitSettings);

        auto injector = di::make_injector(
            di::bind<settings::DbSettings>().to<settings::DbSettings>().in(di::singleton),
            di::bind<settings::RabbitMQSettings>().to(rabbitSettings),
            di::bind<settings::BrokerSettings>().to<settings::BrokerSettings>().in(di::singleton),
            
            di::bind<ports::output::IEventPublisher>().to(rabbitMQAdapter_),
            di::bind<ports::output::IEventConsumer>().to(rabbitMQAdapter_),
            
            di::bind<ports::output::IInstrumentRepository>().to<adapters::secondary::PostgresInstrumentRepository>().in(di::singleton),
            di::bind<ports::output::IQuoteRepository>().to<adapters::secondary::PostgresQuoteRepository>().in(di::singleton),
            di::bind<ports::output::IBrokerOrderRepository>().to<adapters::secondary::PostgresBrokerOrderRepository>().in(di::singleton),
            di::bind<ports::output::IBrokerPositionRepository>().to<adapters::secondary::PostgresBrokerPositionRepository>().in(di::singleton),
            di::bind<ports::output::IBrokerBalanceRepository>().to<adapters::secondary::PostgresBrokerBalanceRepository>().in(di::singleton),
            
            di::bind<adapters::secondary::EnhancedFakeBroker>().to<adapters::secondary::EnhancedFakeBroker>().in(di::singleton),
            di::bind<ports::output::IBrokerGateway>().to<adapters::secondary::FakeBrokerAdapter>().in(di::singleton),
            di::bind<ports::input::IQuoteService>().to<application::QuoteService>().in(di::singleton)
        );

        // Event Handlers
        auto eventConsumer = injector.create<std::shared_ptr<ports::output::IEventConsumer>>();
        auto eventPublisher = injector.create<std::shared_ptr<ports::output::IEventPublisher>>();
        auto brokerGateway = injector.create<std::shared_ptr<ports::output::IBrokerGateway>>();

        orderCommandHandler_ = std::make_shared<application::OrderCommandHandler>(eventConsumer, eventPublisher, brokerGateway);
        marketDataPublisher_ = std::make_shared<application::MarketDataPublisher>(eventPublisher);

        // HTTP Handlers (только GET!)
        handlers_[getHandlerKey("GET", "/health")] = injector.create<std::shared_ptr<adapters::primary::HealthHandler>>();
        handlers_[getHandlerKey("GET", "/metrics")] = injector.create<std::shared_ptr<adapters::primary::MetricsHandler>>();
        
        auto instrHandler = injector.create<std::shared_ptr<adapters::primary::InstrumentsHandler>>();
        handlers_[getHandlerKey("GET", "/api/v1/instruments")] = instrHandler;
        handlers_[getHandlerKey("GET", "/api/v1/instruments/*")] = instrHandler;
        
        handlers_[getHandlerKey("GET", "/api/v1/quotes")] = injector.create<std::shared_ptr<adapters::primary::QuotesHandler>>();
        
        auto portfolioHandler = injector.create<std::shared_ptr<adapters::primary::PortfolioHandler>>();
        handlers_[getHandlerKey("GET", "/api/v1/portfolio")] = portfolioHandler;
        handlers_[getHandlerKey("GET", "/api/v1/portfolio/*")] = portfolioHandler;
        
        auto ordersHandler = injector.create<std::shared_ptr<adapters::primary::OrdersHandler>>();
        handlers_[getHandlerKey("GET", "/api/v1/orders")] = ordersHandler;
        handlers_[getHandlerKey("GET", "/api/v1/orders/*")] = ordersHandler;
        // POST и DELETE убраны - теперь через RabbitMQ!

        std::cout << "[BrokerApp] Ready (POST/DELETE via RabbitMQ)" << std::endl;
    }

private:
    std::shared_ptr<adapters::secondary::RabbitMQAdapter> rabbitMQAdapter_;
    std::shared_ptr<application::OrderCommandHandler> orderCommandHandler_;
    std::shared_ptr<application::MarketDataPublisher> marketDataPublisher_;
};

} // namespace broker
