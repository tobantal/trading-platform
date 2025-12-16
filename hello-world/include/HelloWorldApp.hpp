#pragma once

#include <BoostBeastApplication.hpp>
#include <IHttpHandler.hpp>
#include <memory>

/**
 * @class HelloWorldApp
 * @brief Главное приложение для HelloWorld микросервиса
 * 
 * Наследует BoostBeastApplication с Template Method паттерном:
 * 1. loadEnvironment() - загрузка config.json в Environment
 * 2. configureInjection() - регистрация handlers через Boost.DI
 * 3. start() - запуск HTTP сервера (из базового класса)
 */
class HelloWorldApp : public BoostBeastApplication
{
public:
    HelloWorldApp();
    virtual ~HelloWorldApp();

protected:
    /**
     * @brief Загрузить конфигурацию из окружения
     * 
     * Вызывает базовый BoostBeastApplication::loadEnvironment()
     * для загрузки config.json
     */
    void loadEnvironment(int argc, char* argv[]) override;

    /**
     * @brief Настроить DI контейнер и зарегистрировать handlers
     * 
     * Использует Boost.DI для создания и регистрации handlers:
     * - GET /api/v1/health → HealthCheckHandler
     * - GET /metrics → MetricsHandler  
     * - GET /echo → EchoHandler
     */
    void configureInjection() override;
};