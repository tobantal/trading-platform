#pragma once

#include <BoostBeastApplication.hpp>
#include <IHttpHandler.hpp>
#include <boost/di.hpp>
#include <memory>

// Forward declarations - Ports
namespace trading::ports::input {
    class IAuthService;
    class IMarketService;
    class IOrderService;
    class IPortfolioService;
    class IStrategyService;
    class IAccountService;
}

namespace trading::ports::output {
    class IBrokerGateway;
    class IJwtProvider;
    class ICachePort;
    class IEventBus;
    class IUserRepository;
    class IAccountRepository;
    class IOrderRepository;
    class IStrategyRepository;
}

/**
 * @class TradingApp
 * @brief Главное приложение Trading Platform MVP
 * 
 * Наследует BoostBeastApplication с Template Method паттерном:
 * 1. loadEnvironment() - загрузка config.json в Environment
 * 2. configureInjection() - настройка Boost.DI и регистрация handlers
 * 3. start() - запуск HTTP сервера (из базового класса)
 * 
 * Архитектура: Hexagonal (Ports & Adapters)
 * - Primary Adapters: HTTP Handlers
 * - Secondary Adapters: FakeTinkoff, FakeJwt, LruCache, InMemory*
 * 
 * Dependency Injection: Boost.DI
 * - Биндинги интерфейсов к реализациям
 * - Автоматическое разрешение зависимостей
 * - Singleton scope для stateful адаптеров
 */
class TradingApp : public BoostBeastApplication
{
public:
    TradingApp();
    ~TradingApp() override;

protected:
    /**
     * @brief Загрузить конфигурацию из окружения
     */
    void loadEnvironment(int argc, char* argv[]) override;

    /**
     * @brief Настроить Boost.DI контейнер и зарегистрировать handlers
     * 
     * Использует Boost.DI для:
     * 1. Биндинга Output Ports (интерфейсов) к Secondary Adapters (реализациям)
     * 2. Биндинга Input Ports к Application Services
     * 3. Создания Primary Adapters (Handlers) с автоматическим разрешением зависимостей
     */
    void configureInjection() override;

/**
 * @brief Вывести баннер при старте приложения
 */
private:
    void printStartupBanner();
};
