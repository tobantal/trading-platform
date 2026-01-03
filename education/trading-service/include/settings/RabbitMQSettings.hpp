#pragma once

#include <string>
#include <cstdlib>

namespace trading::settings {

/**
 * @brief Настройки RabbitMQ
 * 
 * Читает из ENV:
 * - RABBITMQ_HOST (default: "rabbitmq")
 * - RABBITMQ_PORT (default: 5672)
 * - RABBITMQ_USER (default: "guest")
 * - RABBITMQ_PASSWORD (default: "guest")
 * - RABBITMQ_EXCHANGE (default: "trading.events")
 */
class RabbitMQSettings {
public:
    RabbitMQSettings() {
        if (const char* host = std::getenv("RABBITMQ_HOST")) {
            host_ = host;
        }
        if (const char* port = std::getenv("RABBITMQ_PORT")) {
            port_ = std::stoi(port);
        }
        if (const char* user = std::getenv("RABBITMQ_USER")) {
            user_ = user;
        }
        if (const char* password = std::getenv("RABBITMQ_PASSWORD")) {
            password_ = password;
        }
        if (const char* exchange = std::getenv("RABBITMQ_EXCHANGE")) {
            exchange_ = exchange;
        }
    }
    
    std::string getHost() const { return host_; }
    int getPort() const { return port_; }
    std::string getUser() const { return user_; }
    std::string getPassword() const { return password_; }
    std::string getExchange() const { return exchange_; }

private:
    std::string host_ = "rabbitmq";
    int port_ = 5672;
    std::string user_ = "guest";
    std::string password_ = "guest";
    std::string exchange_ = "trading.events";
};

} // namespace trading::settings
