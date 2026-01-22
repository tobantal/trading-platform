#pragma once

#include <BoostBeastApplication.hpp>
#include <boost/di.hpp>

// Ports
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include "ports/output/IUserRepository.hpp"
#include "ports/output/IAccountRepository.hpp"
#include "ports/output/ISessionRepository.hpp"
#include "ports/output/IJwtProvider.hpp"

// Application
#include "application/AuthService.hpp"
#include "application/AccountService.hpp"

// Secondary Adapters
#include "adapters/secondary/FakeJwtAdapter.hpp"
#include "adapters/secondary/PostgresUserRepository.hpp"
#include "adapters/secondary/PostgresAccountRepository.hpp"
#include "adapters/secondary/PostgresSessionRepository.hpp"
#include "adapters/secondary/DbSettings.hpp"
#include "adapters/secondary/AuthSettings.hpp"

// Primary Adapters
#include "HealthHandler.hpp"
#include "adapters/primary/MetricsHandler.hpp"
#include "adapters/primary/RegisterHandler.hpp"
#include "adapters/primary/LoginHandler.hpp"
#include "adapters/primary/LogoutHandler.hpp"
#include "adapters/primary/ValidateTokenHandler.hpp"
#include "adapters/primary/GetAccountsHandler.hpp"
#include "adapters/primary/AddAccountHandler.hpp"
#include "adapters/primary/DeleteAccountHandler.hpp"
#include "adapters/primary/GetAccessTokenHandler.hpp"

#include <memory>
#include <iostream>

namespace di = boost::di;

namespace auth {

/**
 * @brief Auth Service Application
 * 
 * Точка входа для Auth микросервиса.
 * Настраивает Boost.DI контейнер и регистрирует HTTP handlers.
 */
class AuthApp : public BoostBeastApplication {
public:
    AuthApp() {
        std::cout << "[AuthApp] Initializing..." << std::endl;
    }
    
    ~AuthApp() override = default;

protected:
    void loadEnvironment(int argc, char* argv[]) override {
        BoostBeastApplication::loadEnvironment(argc, argv);
        std::cout << "[AuthApp] Environment loaded" << std::endl;
    }

    void configureInjection() override {
        std::cout << "[AuthApp] Configuring Boost.DI injection..." << std::endl;

        // ====================================================================
        // Boost.DI Injector Configuration
        // ====================================================================

        auto injector = di::make_injector(

            // ================================================================
            // Layer 1: Settings & Infrastructure
            // ================================================================
            di::bind<adapters::secondary::DbSettings>()
                .to(std::make_shared<adapters::secondary::DbSettings>()),

            di::bind<adapters::secondary::AuthSettings>()
                .to(std::make_shared<adapters::secondary::AuthSettings>()),

            // ================================================================
            // Layer 2: Secondary Adapters (Output Ports implementations)
            // ================================================================
            
            di::bind<ports::output::IUserRepository>()
                .to<adapters::secondary::PostgresUserRepository>()
                .in(di::singleton),
            
            di::bind<ports::output::IAccountRepository>()
                .to<adapters::secondary::PostgresAccountRepository>()
                .in(di::singleton),
            
            di::bind<ports::output::ISessionRepository>()
                .to<adapters::secondary::PostgresSessionRepository>()
                .in(di::singleton),
            
            di::bind<ports::output::IJwtProvider>()
                .to<adapters::secondary::FakeJwtAdapter>()
                .in(di::singleton),

            // ================================================================
            // Layer 3: Application Services (Input Ports implementations)
            // ================================================================
            
            di::bind<ports::input::IAuthService>()
                .to<application::AuthService>()
                .in(di::singleton),
            
            di::bind<ports::input::IAccountService>()
                .to<application::AccountService>()
                .in(di::singleton)
        );

        std::cout << "[AuthApp] DI Injector configured:" << std::endl;
        std::cout << "  ✓ Secondary Adapters (4 bindings)" << std::endl;
        std::cout << "  ✓ Application Services (2 bindings)" << std::endl;

        // ====================================================================
        // Layer 4: Primary Adapters (HTTP Handlers)
        // ====================================================================
        
        std::cout << "[AuthApp] Registering HTTP Handlers via DI..." << std::endl;

        // Health & Metrics (без зависимостей)
        {
            auto handler = injector.create<std::shared_ptr<HealthHandler>>();
            registerEndpoint("GET", "/health", handler);
            std::cout << "  ✓ HealthHandler: GET /health" << std::endl;
        }

        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::MetricsHandler>>();
            registerEndpoint("GET", "/metrics", handler);
            std::cout << "  ✓ MetricsHandler: GET /metrics" << std::endl;
        }

        // Auth Handlers
        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::RegisterHandler>>();
            registerEndpoint("POST", "/api/v1/auth/register", handler);
            std::cout << "  ✓ RegisterHandler: POST /api/v1/auth/register" << std::endl;
        }

        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::LoginHandler>>();
            registerEndpoint("POST", "/api/v1/auth/login", handler);
            std::cout << "  ✓ LoginHandler: POST /api/v1/auth/login" << std::endl;
        }

        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::LogoutHandler>>();
            registerEndpoint("POST", "/api/v1/auth/logout", handler);
            std::cout << "  ✓ LogoutHandler: POST /api/v1/auth/logout" << std::endl;
        }

        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::ValidateTokenHandler>>();
            registerEndpoint("POST", "/api/v1/auth/validate", handler);
            std::cout << "  ✓ ValidateTokenHandler: POST /api/v1/auth/validate" << std::endl;
        }

        // Access Token Handler
        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::GetAccessTokenHandler>>();
            registerEndpoint("POST", "/api/v1/auth/access-token", handler);
            std::cout << "  ✓ GetAccessTokenHandler: POST /api/v1/auth/access-token" << std::endl;
        }

        // Account Handlers
        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::GetAccountsHandler>>();
            registerEndpoint("GET", "/api/v1/accounts", handler);
            std::cout << "  ✓ GetAccountsHandler: GET /api/v1/accounts" << std::endl;
        }

        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::AddAccountHandler>>();
            registerEndpoint("POST", "/api/v1/accounts", handler);
            std::cout << "  ✓ AddAccountHandler: POST /api/v1/accounts" << std::endl;
        }

        {
            auto handler = injector.create<std::shared_ptr<adapters::primary::DeleteAccountHandler>>();
            registerEndpoint("DELETE", "/api/v1/accounts/*", handler);
            std::cout << "  ✓ DeleteAccountHandler: DELETE /api/v1/accounts/*" << std::endl;
        }

        std::cout << "[AuthApp] Configuration complete! 10 handlers registered." << std::endl;
    }
};

} // namespace auth
