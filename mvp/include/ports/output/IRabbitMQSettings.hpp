// include/ports/output/IRabbitMQSettings.hpp
#pragma once

#include <string>

namespace trading::ports::output {

/**
 * @brief Интерфейс настроек подключения к RabbitMQ
 * 
 * Следует паттерну IDbSettings из microservice-core.
 * Реализация получает значения из IEnvironment.
 */
class IRabbitMQSettings {
public:
    virtual ~IRabbitMQSettings() = default;
    
    /// Хост RabbitMQ
    virtual std::string getHost() const = 0;
    
    /// Порт RabbitMQ (по умолчанию 5672)
    virtual int getPort() const = 0;
    
    /// Имя пользователя
    virtual std::string getUser() const = 0;
    
    /// Пароль
    virtual std::string getPassword() const = 0;
    
    /// Virtual host (по умолчанию "/")
    virtual std::string getVHost() const = 0;
    
    /// Exchange name (по умолчанию "trading.events")
    virtual std::string getExchange() const = 0;
};

} // namespace trading::ports::output