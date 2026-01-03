#pragma once

#include <string>
#include <cstdlib>

namespace trading::settings {

/**
 * @brief Настройки подключения к Auth Service
 * 
 * Читает из ENV:
 * - AUTH_SERVICE_HOST (default: "auth-service")
 * - AUTH_SERVICE_PORT (default: 8080)
 */
class AuthClientSettings {
public:
    AuthClientSettings() {
        if (const char* host = std::getenv("AUTH_SERVICE_HOST")) {
            host_ = host;
        }
        if (const char* port = std::getenv("AUTH_SERVICE_PORT")) {
            port_ = std::stoi(port);
        }
    }
    
    std::string getHost() const { return host_; }
    int getPort() const { return port_; }

private:
    std::string host_ = "auth-service";
    int port_ = 8080;
};

} // namespace trading::settings
