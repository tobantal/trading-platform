// include/settings/RabbitMQSettings.hpp
#pragma once

#include <string>
#include <cstdlib>
#include <stdexcept>

namespace broker::settings {

/**
 * @brief Настройки подключения к RabbitMQ
 * 
 * Читает параметры из переменных окружения (K8s ENV).
 * 
 * Переменные:
 * - RABBITMQ_HOST: хост (по умолчанию "rabbitmq")
 * - RABBITMQ_PORT: порт (по умолчанию 5672)
 * - RABBITMQ_USER: пользователь (по умолчанию "guest")
 * - RABBITMQ_PASSWORD: пароль (обязательный)
 * - RABBITMQ_EXCHANGE: имя exchange (по умолчанию "broker.events")
 */
class RabbitMQSettings {
public:
    RabbitMQSettings() {
        host_ = getEnvOrDefault("RABBITMQ_HOST", "rabbitmq");
        port_ = std::stoi(getEnvOrDefault("RABBITMQ_PORT", "5672"));
        user_ = getEnvOrDefault("RABBITMQ_USER", "guest");
        password_ = getEnvOrDefault("RABBITMQ_PASSWORD", "guest");
        exchange_ = getEnvOrDefault("RABBITMQ_EXCHANGE", "broker.events");
    }
    
    std::string getHost() const { return host_; }
    int getPort() const { return port_; }
    std::string getUser() const { return user_; }
    std::string getPassword() const { return password_; }
    std::string getExchange() const { return exchange_; }

private:
    std::string host_;
    int port_;
    std::string user_;
    std::string password_;
    std::string exchange_;
    
    static std::string getEnvOrDefault(const char* name, const char* defaultValue) {
        const char* value = std::getenv(name);
        return value ? std::string(value) : std::string(defaultValue);
    }
};

} // namespace broker::settings
