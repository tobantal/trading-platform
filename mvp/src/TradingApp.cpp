#include "TradingApp.hpp"

#include <IEnvironment.hpp>

// Auth Handlers (Primary Adapters)
#include "adapters/primary/auth/LoginHandler.hpp"
#include "adapters/primary/auth/SelectAccountHandler.hpp"
#include "adapters/primary/auth/ValidateTokenHandler.hpp"
#include "adapters/primary/auth/RefreshTokenHandler.hpp"
#include "adapters/primary/auth/LogoutHandler.hpp"
#include "adapters/primary/auth/RegisterHandler.hpp"

// Account Handlers  (Primary Adapters)
#include "adapters/primary/account/GetAccountsHandler.hpp"
#include "adapters/primary/account/AddAccountHandler.hpp"
#include "adapters/primary/account/DeleteAccountHandler.hpp"

// Handlers (Primary Adapters)
#include "adapters/primary/MarketHandler.hpp"
#include "adapters/primary/OrderHandler.hpp"
#include "adapters/primary/PortfolioHandler.hpp"
#include "adapters/primary/StrategyHandler.hpp"
#include "adapters/primary/HealthHandler.hpp"
#include "adapters/primary/MetricsHandler.hpp"

// Application Services
#include "application/AuthService.hpp"
#include "application/AccountService.hpp"
#include "application/MarketService.hpp"
#include "application/OrderService.hpp"
#include "application/PortfolioService.hpp"
#include "application/StrategyService.hpp"

// Secondary Adapters
#include "adapters/secondary/broker/FakeTinkoffAdapter.hpp"
#include "adapters/secondary/auth/FakeJwtAdapter.hpp"
#include "adapters/secondary/cache/LruCacheAdapter.hpp"
#include "adapters/secondary/events/RabbitMQEventBus.hpp"
#include "adapters/secondary/settings/RabbitMQSettings.hpp"
#include "adapters/secondary/persistence/InMemoryUserRepository.hpp"
#include "adapters/secondary/persistence/InMemoryAccountRepository.hpp"
#include "adapters/secondary/persistence/InMemoryOrderRepository.hpp"
#include "adapters/secondary/persistence/InMemoryStrategyRepository.hpp"

// Ports (Output)
#include "ports/output/IRabbitMQSettings.hpp"

// Domain Event Factory
#include "domain/events/DomainEventFactory.hpp"
#include "application/events/SimpleDomainEventFactory.hpp"

#include <iostream>

namespace di = boost::di;

// ============================================================================
// Конфигурационные константы для DI
// ============================================================================
namespace config
{
    constexpr int JWT_LIFETIME_SECONDS = 3600; // 1 час
    constexpr size_t CACHE_CAPACITY = 10000;   // элементов
    constexpr int CACHE_TTL_SECONDS = 5;       // 5 секунд
}

// ============================================================================
// TradingApp Implementation
// ============================================================================

TradingApp::TradingApp()
{
    std::cout << "[TradingApp] Application created" << std::endl;
}

TradingApp::~TradingApp()
{
    std::cout << "[TradingApp] Application destroyed" << std::endl;
}

void TradingApp::loadEnvironment(int argc, char *argv[])
{
    std::cout << "[TradingApp] Loading environment..." << std::endl;

    // Вызываем базовый метод который загружает config.json в env_
    BoostBeastApplication::loadEnvironment(argc, argv);

    std::cout << "[TradingApp] Environment loaded successfully" << std::endl;
}

void TradingApp::configureInjection()
{
    printStartupBanner();

    std::cout << "[TradingApp] Configuring Boost.DI injection..." << std::endl;

    // ========================================================================
    // Boost.DI Injector Configuration
    // ========================================================================

    auto injector = di::make_injector(

        // ====================================================================
        // Layer 1: Secondary Adapters (Output Ports implementations)
        // ====================================================================

        // IEnvironment
        di::bind<IEnvironment>().to(env_),

        // IRabbitMQSettings ← RabbitMQSettings(IEnvironment)
        di::bind<trading::ports::output::IRabbitMQSettings>()
            .to<trading::adapters::secondary::RabbitMQSettings>()
            .in(di::singleton),

        // DomainEventFactory ← SimpleDomainEventFactory (авторегистрация в конструкторе)
        di::bind<trading::domain::DomainEventFactory>()
            .to<trading::application::SimpleDomainEventFactory>()
            .in(di::singleton),

        // IEventBus ← RabbitMQEventBus(IRabbitMQSettings, DomainEventFactory)
        di::bind<trading::ports::output::IEventBus>()
            .to<trading::adapters::secondary::RabbitMQEventBus>()
            .in(di::singleton),

        // Broker Gateway - подключение к бирже (fake для MVP)
        di::bind<trading::ports::output::IBrokerGateway>()
            .to<trading::adapters::secondary::FakeTinkoffAdapter>()
            .in(di::singleton),

        // JWT Provider - аутентификация (fake для MVP)
        di::bind<trading::ports::output::IJwtProvider>()
            .to(std::make_shared<trading::adapters::secondary::FakeJwtAdapter>(
                config::JWT_LIFETIME_SECONDS)),

        // Cache - LRU кэш для котировок
        di::bind<trading::ports::output::ICachePort>()
            .to(std::make_shared<trading::adapters::secondary::LruCacheAdapter>(
                config::CACHE_CAPACITY,
                config::CACHE_TTL_SECONDS)),

        // Repositories - in-memory хранилища
        di::bind<trading::ports::output::IUserRepository>()
            .to<trading::adapters::secondary::InMemoryUserRepository>()
            .in(di::singleton),

        di::bind<trading::ports::output::IAccountRepository>()
            .to<trading::adapters::secondary::InMemoryAccountRepository>()
            .in(di::singleton),

        di::bind<trading::ports::output::IOrderRepository>()
            .to<trading::adapters::secondary::InMemoryOrderRepository>()
            .in(di::singleton),

        di::bind<trading::ports::output::IStrategyRepository>()
            .to<trading::adapters::secondary::InMemoryStrategyRepository>()
            .in(di::singleton),

        // ====================================================================
        // Layer 2: Application Services (Input Ports implementations)
        // ====================================================================

        di::bind<trading::ports::input::IAuthService>()
            .to<trading::application::AuthService>()
            .in(di::singleton),

        di::bind<trading::ports::input::IAccountService>()
            .to<trading::application::AccountService>()
            .in(di::singleton),

        di::bind<trading::ports::input::IMarketService>()
            .to<trading::application::MarketService>()
            .in(di::singleton),

        di::bind<trading::ports::input::IOrderService>()
            .to<trading::application::OrderService>()
            .in(di::singleton),

        di::bind<trading::ports::input::IPortfolioService>()
            .to<trading::application::PortfolioService>()
            .in(di::singleton),

        di::bind<trading::ports::input::IStrategyService>()
            .to<trading::application::StrategyService>()
            .in(di::singleton));

    std::cout << "\n📦 Boost.DI Injector configured:" << std::endl;
    std::cout << "  ✓ Secondary Adapters (8 bindings)" << std::endl;
    std::cout << "  ✓ Application Services (6 bindings)" << std::endl;

    // ========================================================================
    // Layer 3: Primary Adapters (HTTP Handlers)
    // ========================================================================

    std::cout << "\n🎮 Registering HTTP Handlers via DI..." << std::endl;

    // ========================================================================
    // AUTH HANDLERS
    // ========================================================================
    {
        auto loginHandler = injector.create<
            std::shared_ptr<trading::adapters::primary::LoginHandler>>();
        handlers_[getHandlerKey("POST", "/api/v1/auth/login")] = loginHandler;
        std::cout << "  ✓ LoginHandler: POST /api/v1/auth/login" << std::endl;
    }

    {
        auto selectAccountHandler = injector.create<
            std::shared_ptr<trading::adapters::primary::SelectAccountHandler>>();
        handlers_[getHandlerKey("POST", "/api/v1/auth/select-account")] = selectAccountHandler;
        std::cout << "  ✓ SelectAccountHandler: POST /api/v1/auth/select-account" << std::endl;
    }

    {
        auto validateHandler = injector.create<
            std::shared_ptr<trading::adapters::primary::ValidateTokenHandler>>();
        handlers_[getHandlerKey("POST", "/api/v1/auth/validate")] = validateHandler;
        std::cout << "  ✓ ValidateTokenHandler: POST /api/v1/auth/validate" << std::endl;
    }

    {
        auto refreshHandler = injector.create<
            std::shared_ptr<trading::adapters::primary::RefreshTokenHandler>>();
        handlers_[getHandlerKey("POST", "/api/v1/auth/refresh")] = refreshHandler;
        std::cout << "  ✓ RefreshTokenHandler: POST /api/v1/auth/refresh" << std::endl;
    }

    {
        auto logoutHandler = injector.create<
            std::shared_ptr<trading::adapters::primary::LogoutHandler>>();
        handlers_[getHandlerKey("POST", "/api/v1/auth/logout")] = logoutHandler;
        std::cout << "  ✓ LogoutHandler: POST /api/v1/auth/logout" << std::endl;
    }

    {
        auto handler = injector.create<std::shared_ptr<trading::adapters::primary::RegisterHandler>>();
        handlers_[getHandlerKey("POST", "/api/v1/auth/register")] = handler;
        std::cout << "  ✓ RegisterHandler: POST /api/v1/auth/register" << std::endl;
    }

    // ========================================================================
    // ACCOUNT HANDLERS
    // ========================================================================
    {
        auto getHandler = injector.create<std::shared_ptr<trading::adapters::primary::GetAccountsHandler>>();
        handlers_[getHandlerKey("GET", "/api/v1/accounts")] = getHandler;

        auto addHandler = injector.create<std::shared_ptr<trading::adapters::primary::AddAccountHandler>>();
        handlers_[getHandlerKey("POST", "/api/v1/accounts")] = addHandler;

        auto deleteHandler = injector.create<std::shared_ptr<trading::adapters::primary::DeleteAccountHandler>>();
        handlers_[getHandlerKey("DELETE", "/api/v1/accounts/*")] = deleteHandler;
        
        std::cout << "  ✓ AccountHandlers: GET/POST/DELETE /api/v1/accounts" << std::endl;
    }

    // ========================================================================
    // BUSINESS HANDLERS
    // ========================================================================
    
    // Market Handler
    {
        auto handler = injector.create<std::shared_ptr<trading::adapters::primary::MarketHandler>>();
        handlers_[getHandlerKey("GET", "/api/v1/quotes")] = handler;
        handlers_[getHandlerKey("GET", "/api/v1/instruments")] = handler;
        handlers_[getHandlerKey("GET", "/api/v1/instruments/search")] = handler;
        handlers_[getHandlerKey("GET", "/api/v1/instruments/*")] = handler;
        std::cout << "  ✓ MarketHandler: GET /api/v1/quotes, /api/v1/instruments" << std::endl;
    }

    // Order Handler
    {
        auto handler = injector.create<std::shared_ptr<trading::adapters::primary::OrderHandler>>();
        handlers_[getHandlerKey("POST", "/api/v1/orders")] = handler;
        handlers_[getHandlerKey("GET", "/api/v1/orders")] = handler;
        handlers_[getHandlerKey("GET", "/api/v1/orders/*")] = handler;
        handlers_[getHandlerKey("DELETE", "/api/v1/orders/*")] = handler;
        std::cout << "  ✓ OrderHandler: POST/GET/DELETE /api/v1/orders" << std::endl;
    }

    // Portfolio Handler
    {
        auto handler = injector.create<std::shared_ptr<trading::adapters::primary::PortfolioHandler>>();
        handlers_[getHandlerKey("GET", "/api/v1/portfolio")] = handler;
        handlers_[getHandlerKey("GET", "/api/v1/portfolio/positions")] = handler;
        handlers_[getHandlerKey("GET", "/api/v1/portfolio/cash")] = handler;
        std::cout << "  ✓ PortfolioHandler: GET /api/v1/portfolio" << std::endl;
    }

    // Strategy Handler
    {
        auto handler = injector.create<std::shared_ptr<trading::adapters::primary::StrategyHandler>>();
        handlers_[getHandlerKey("POST", "/api/v1/strategies")] = handler;
        handlers_[getHandlerKey("GET", "/api/v1/strategies")] = handler;
        handlers_[getHandlerKey("GET", "/api/v1/strategies/*")] = handler;
        handlers_[getHandlerKey("POST", "/api/v1/strategies/*/start")] = handler;
        handlers_[getHandlerKey("POST", "/api/v1/strategies/*/stop")] = handler;
        handlers_[getHandlerKey("DELETE", "/api/v1/strategies/*")] = handler;
        std::cout << "  ✓ StrategyHandler: POST/GET/DELETE /api/v1/strategies" << std::endl;
    }

    // ========================================================================
    // INFRASTRUCTURE HANDLERS
    // ========================================================================
    
    // Health Handler
    {
        auto handler = injector.create<std::shared_ptr<trading::adapters::primary::HealthHandler>>();
        handlers_[getHandlerKey("GET", "/api/v1/health")] = handler;
        std::cout << "  ✓ HealthHandler: GET /api/v1/health" << std::endl;
    }

    // Metrics Handler — через DI, подписка на события в конструкторе
    {
        auto handler = injector.create<std::shared_ptr<trading::adapters::primary::MetricsHandler>>();
        handlers_[getHandlerKey("GET", "/metrics")] = handler;
        std::cout << "  ✓ MetricsHandler: GET /metrics (with EventBus via DI)" << std::endl;
    }

    std::cout << "\n[TradingApp] DI configuration completed - "
              << handlers_.size() << " routes registered" << std::endl;
}

void TradingApp::printStartupBanner()
{
    std::cout << std::endl;
    std::cout << "╔══════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║     Trading Platform MVP — Microservice Edition      ║" << std::endl;
    std::cout << "║                                                      ║" << std::endl;
    std::cout << "║  Architecture: Hexagonal (Ports & Adapters)          ║" << std::endl;
    std::cout << "║  DI Framework: Boost.DI                              ║" << std::endl;
    std::cout << "║  HTTP Server:  Boost.Beast                           ║" << std::endl;
    std::cout << "║  Course: OTUS Microservice Architecture              ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
}