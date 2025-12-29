// include/adapters/secondary/settings/RabbitMQSettings.hpp
#pragma once

#include "ports/output/IRabbitMQSettings.hpp"
#include <IEnvironment.hpp>
#include <memory>
#include <string>

namespace trading::adapters::secondary {

/**
 * @brief Реализация IRabbitMQSettings, получает данные из IEnvironment
 * 
 * Ключи в config.json / Environment:
 * - rabbitmq.host     (default: "localhost")
 * - rabbitmq.port     (default: 5672)
 * - rabbitmq.user     (default: "guest")
 * - rabbitmq.password (default: "guest")
 * - rabbitmq.vhost    (default: "/")
 * - rabbitmq.exchange (default: "trading.events")
 * 
 * Пример config.json:
 * {
 *   "rabbitmq.host": "rabbitmq",
 *   "rabbitmq.port": 5672,
 *   "rabbitmq.user": "trading",
 *   "rabbitmq.password": "trading123",
 *   "rabbitmq.vhost": "/trading",
 *   "rabbitmq.exchange": "trading.events"
 * }
 */
class RabbitMQSettings : public ports::output::IRabbitMQSettings {
public:
    /**
     * @brief Конструктор с валидацией
     * @param env Окружение с настройками
     */
    explicit RabbitMQSettings(std::shared_ptr<IEnvironment> env)
        : host_(env->get<std::string>("rabbitmq.host", "localhost"))
        , port_(env->get<int>("rabbitmq.port", 5672))
        , user_(env->get<std::string>("rabbitmq.user", "guest"))
        , password_(env->get<std::string>("rabbitmq.password", "guest"))
        , vhost_(env->get<std::string>("rabbitmq.vhost", "/"))
        , exchange_(env->get<std::string>("rabbitmq.exchange", "trading.events"))
    {
    }
    
    std::string getHost() const override { return host_; }
    int getPort() const override { return port_; }
    std::string getUser() const override { return user_; }
    std::string getPassword() const override { return password_; }
    std::string getVHost() const override { return vhost_; }
    std::string getExchange() const override { return exchange_; }

private:
    std::string host_;
    int port_;
    std::string user_;
    std::string password_;
    std::string vhost_;
    std::string exchange_;
};

} // namespace trading::adapters::secondary
