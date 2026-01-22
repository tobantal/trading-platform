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

#include "adapters/primary/GetAllInstrumentsHandler.hpp"
#include "adapters/primary/GetInstrumentHandler.hpp"

#include "adapters/primary/QuotesHandler.hpp"

#include "adapters/primary/GetOrderHandler.hpp"
#include "adapters/primary/GetOrdersHandler.hpp"

#include "adapters/primary/GetPortfolioHandler.hpp"
#include "adapters/primary/GetPositionsHandler.hpp"
#include "adapters/primary/GetCashHandler.hpp"



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

        // Шаг 1: Создаём RabbitMQAdapter через DI (один экземпляр для Publisher и Consumer)
        auto rabbitInjector = di::make_injector(
            di::bind<settings::RabbitMQSettings>().in(di::singleton)
        );
        auto rabbitMQAdapter = rabbitInjector.create<std::shared_ptr<adapters::secondary::RabbitMQAdapter>>();
        
        // Шаг 2: Основной injector с instance binding для RabbitMQ
        auto injector = di::make_injector(
            di::bind<settings::DbSettings>().in(di::singleton),
            di::bind<settings::RabbitMQSettings>().in(di::singleton),
            
            // RabbitMQ - один экземпляр для обоих интерфейсов
            di::bind<ports::output::IEventPublisher>().to(rabbitMQAdapter),
            di::bind<ports::output::IEventConsumer>().to(rabbitMQAdapter),
            
            di::bind<ports::output::IInstrumentRepository>().to<adapters::secondary::PostgresInstrumentRepository>().in(di::singleton),
            di::bind<ports::output::IQuoteRepository>().to<adapters::secondary::PostgresQuoteRepository>().in(di::singleton),
            di::bind<ports::output::IBrokerOrderRepository>().to<adapters::secondary::PostgresBrokerOrderRepository>().in(di::singleton),
            di::bind<ports::output::IBrokerPositionRepository>().to<adapters::secondary::PostgresBrokerPositionRepository>().in(di::singleton),
            di::bind<ports::output::IBrokerBalanceRepository>().to<adapters::secondary::PostgresBrokerBalanceRepository>().in(di::singleton),
            di::bind<adapters::secondary::EnhancedFakeBroker>().in(di::singleton),
            di::bind<ports::output::IBrokerGateway>().to<adapters::secondary::FakeBrokerAdapter>().in(di::singleton),
            di::bind<ports::input::IQuoteService>().to<application::QuoteService>().in(di::singleton)
        );

        // Настройки брокера (определяюет его поведение)
        auto brokerSettings = injector.create<std::shared_ptr<settings::BrokerSettings>>();
        // Продвинутый эмитатор биржи
        auto enhancedFakeBroker = injector.create<std::shared_ptr<adapters::secondary::EnhancedFakeBroker>>();

        // Шаг 3: Event Handlers через DI
        // OrderCommandHandler вызывает subscribe() в конструкторе
        auto orderCommandHandler_ = injector.create<std::shared_ptr<application::OrderCommandHandler>>();
        auto marketDataPublisher_ = injector.create<std::shared_ptr<application::MarketDataPublisher>>();

        // Шаг 4: Запускаем RabbitMQ ПОСЛЕ регистрации всех handlers
        std::cout << "[BrokerApp] Starting RabbitMQ consumer..." << std::endl;
        rabbitMQAdapter->start();

        // HTTP Handlers (только GET!)
        registerEndpoint("GET", "/health", injector.create<std::shared_ptr<adapters::primary::HealthHandler>>());
        registerEndpoint("GET", "/metrics", injector.create<std::shared_ptr<adapters::primary::MetricsHandler>>());
        
        registerEndpoint("GET", "/api/v1/instruments", injector.create<std::shared_ptr<adapters::primary::GetAllInstrumentsHandler>>());
        registerEndpoint("GET", "/api/v1/instruments/*", injector.create<std::shared_ptr<adapters::primary::GetInstrumentHandler>>());

        registerEndpoint("GET", "/api/v1/quotes", injector.create<std::shared_ptr<adapters::primary::QuotesHandler>>());
        
        registerEndpoint("GET", "/api/v1/portfolio", injector.create<std::shared_ptr<adapters::primary::GetPortfolioHandler>>());
        registerEndpoint("GET", "/api/v1/portfolio/positions", injector.create<std::shared_ptr<adapters::primary::GetPositionsHandler>>());
        registerEndpoint("GET", "/api/v1/portfolio/cash", injector.create<std::shared_ptr<adapters::primary::GetCashHandler>>());

        registerEndpoint("GET", "/api/v1/orders", injector.create<std::shared_ptr<adapters::primary::GetOrdersHandler>>());
        registerEndpoint("GET", "/api/v1/orders/*", injector.create<std::shared_ptr<adapters::primary::GetOrderHandler>>());

        std::cout << "[BrokerApp] Ready (POST/DELETE via RabbitMQ)" << std::endl;

        enhancedFakeBroker->startSimulation(std::chrono::milliseconds{brokerSettings->getTickIntervalMs()});
        std::cout << "[BrokerApp] fake broker simulation started" << std::endl;
    }
};

} // namespace broker
