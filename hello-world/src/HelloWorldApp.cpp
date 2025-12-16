#include "HelloWorldApp.hpp"
#include "handlers/HealthCheckHandler.hpp"
#include "handlers/MetricsHandler.hpp"
#include "handlers/EchoHandler.hpp"
#include <iostream>
#include <boost/di.hpp>

namespace di = boost::di;

HelloWorldApp::HelloWorldApp()
{
    std::cout << "[HelloWorldApp] Application created" << std::endl;
}

HelloWorldApp::~HelloWorldApp()
{
    std::cout << "[HelloWorldApp] Application destroyed" << std::endl;
}

void HelloWorldApp::loadEnvironment(int argc, char* argv[])
{
    std::cout << "[HelloWorldApp] Loading environment..." << std::endl;
    
    // Вызываем базовый метод который загружает config.json в env_
    BoostBeastApplication::loadEnvironment(argc, argv);
    
    std::cout << "[HelloWorldApp] Environment loaded successfully" << std::endl;
}

void HelloWorldApp::configureInjection()
{
    std::cout << "[HelloWorldApp] Configuring dependency injection..." << std::endl;

    // Создаём DI injector
    auto injector = di::make_injector(
        // Регистрируем IEnvironment (уже создан в BoostBeastApplication)
        di::bind<IEnvironment>().to(env_),
        
        // Регистрируем handlers как синглтоны
        //FIXME: зачем тут два раза регистрируются хэндлеры?
        di::bind<IHttpHandler>()
            .named("health")
            .to<HealthCheckHandler>()
            .in(di::singleton),
        
        di::bind<IHttpHandler>()
            .named("metrics")
            .to<MetricsHandler>()
            .in(di::singleton),
        
        di::bind<IHttpHandler>()
            .named("echo")
            .to<EchoHandler>()
            .in(di::singleton)
    );

    // Создаём handlers из DI контейнера и регистрируем в handlers_ map
    {
        auto handler = injector.create<std::shared_ptr<HealthCheckHandler>>();
        handlers_[getHandlerKey("GET", "/api/v1/health")] = handler;
        std::cout << "[HelloWorldApp] Registered: GET /api/v1/health" << std::endl;
    }

    {
        auto handler = injector.create<std::shared_ptr<MetricsHandler>>();
        handlers_[getHandlerKey("GET", "/metrics")] = handler;
        std::cout << "[HelloWorldApp] Registered: GET /metrics" << std::endl;
    }

    {
        auto handler = injector.create<std::shared_ptr<EchoHandler>>();
        handlers_[getHandlerKey("GET", "/echo")] = handler;
        std::cout << "[HelloWorldApp] Registered: GET /echo" << std::endl;
    }

    std::cout << "[HelloWorldApp] DI configuration completed" << std::endl;
}