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
#include "application/OrderExecutor.hpp"
#include "application/OrderEventHandler.hpp"

// Secondary Adapters
#include "adapters/secondary/PostgresInstrumentRepository.hpp"
#include "adapters/secondary/PostgresQuoteRepository.hpp"
#include "adapters/secondary/PostgresBrokerOrderRepository.hpp"
#include "adapters/secondary/PostgresBrokerPositionRepository.hpp"
#include "adapters/secondary/PostgresBrokerBalanceRepository.hpp"
#include "adapters/secondary/broker/FakeBrokerAdapter.hpp"

// Primary Adapters
#include "adapters/primary/HealthHandler.hpp"
#include "adapters/primary/MetricsHandler.hpp"
#include "adapters/primary/InstrumentsHandler.hpp"
#include "adapters/primary/QuotesHandler.hpp"

// RabbitMQ
#include "adapters/secondary/events/RabbitMQAdapter.hpp"

#include <iostream>
#include <memory>

namespace di = boost::di;

namespace broker {

/**
 * @brief Broker Service Application
 * 
 * Hexagonal architecture with Boost.DI for dependency injection.
 */
class BrokerApp : public BoostBeastApplication {
public:
    BrokerApp() {
        std::cout << "[BrokerApp] Initializing..." << std::endl;
    }
    
    ~BrokerApp() override = default;

protected:
    void loadEnvironment(int argc, char* argv[]) override {
        BoostBeastApplication::loadEnvironment(argc, argv);
        std::cout << "[BrokerApp] Environment loaded" << std::endl;
    }

    void configureInjection() override {
        std::cout << "[BrokerApp] Configuring Boost.DI injection..." << std::endl;

        auto injector = di::make_injector(
            // Layer 1: Settings
            di::bind<settings::DbSettings>()
                .to<settings::DbSettings>()
                .in(di::singleton),

            di::bind<settings::RabbitMQSettings>()
                .to<settings::RabbitMQSettings>()
                .in(di::singleton),

            di::bind<settings::BrokerSettings>()
                .to<settings::BrokerSettings>()
                .in(di::singleton),

            // events
            di::bind<ports::output::IEventPublisher>()
                .to<adapters::secondary::RabbitMQAdapter>()
                .in(di::singleton),

            // Layer 2: Secondary Adapters (Repositories)
            di::bind<ports::output::IInstrumentRepository>()
                .to<adapters::secondary::PostgresInstrumentRepository>()
                .in(di::singleton),

            di::bind<ports::output::IQuoteRepository>()
                .to<adapters::secondary::PostgresQuoteRepository>()
                .in(di::singleton),

            di::bind<ports::output::IBrokerOrderRepository>()
                .to<adapters::secondary::PostgresBrokerOrderRepository>()
                .in(di::singleton),

            di::bind<ports::output::IBrokerPositionRepository>()
                .to<adapters::secondary::PostgresBrokerPositionRepository>()
                .in(di::singleton),

            di::bind<ports::output::IBrokerBalanceRepository>()
                .to<adapters::secondary::PostgresBrokerBalanceRepository>()
                .in(di::singleton),

            // Layer 3: Broker Gateway (EnhancedFakeBroker с симуляцией цен)
            di::bind<ports::output::IBrokerGateway>()
                .to<adapters::secondary::FakeBrokerAdapter>()
                .in(di::singleton),

            // Layer 4: Application Services
            di::bind<ports::input::IQuoteService>()
                .to<application::QuoteService>()
                .in(di::singleton)
        );

        std::cout << "[BrokerApp] DI Injector configured" << std::endl;

        // Layer 5: HTTP Handlers (Primary Adapters)
        std::cout << "[BrokerApp] Registering HTTP Handlers..." << std::endl;

        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::HealthHandler>>();
            handlers_[getHandlerKey("GET", "/health")] = handler;
            std::cout << "  + HealthHandler: GET /health" << std::endl;
        }

        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::MetricsHandler>>();
            handlers_[getHandlerKey("GET", "/metrics")] = handler;
            std::cout << "  + MetricsHandler: GET /metrics" << std::endl;
        }

        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::InstrumentsHandler>>();
            handlers_[getHandlerKey("GET", "/api/v1/instruments")] = handler;
            handlers_[getHandlerKey("GET", "/api/v1/instruments/*")] = handler;
            std::cout << "  + InstrumentsHandler: GET /api/v1/instruments[/*]" << std::endl;
        }

        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::QuotesHandler>>();
            handlers_[getHandlerKey("GET", "/api/v1/quotes")] = handler;
            std::cout << "  + QuotesHandler: GET /api/v1/quotes" << std::endl;
        }

        std::cout << "[BrokerApp] HTTP Handlers registered" << std::endl;
    }
};

} // namespace broker
