#pragma once

#include <settings/IServerSettings.hpp>
#include <string>
#include <cstdlib>

namespace trading::settings {

/**
 * @brief Настройки сервера
 */
class ServerSettings : public IServerSettings {
public:
    ServerSettings() {
        if (const char* host = std::getenv("SERVER_HOST")) {
            host_ = host;
        }
        if (const char* port = std::getenv("SERVER_PORT")) {
            port_ = static_cast<uint16_t>(std::stoi(port));
        }
    }
    
    std::string getHost() const override { return host_; }
    uint16_t getPort() const override { return port_; }

private:
    std::string host_ = "0.0.0.0";
    uint16_t port_ = 8080;
};

} // namespace trading::settings
