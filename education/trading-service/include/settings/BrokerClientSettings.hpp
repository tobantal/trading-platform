#pragma once

#include "settings/IBrokerClientSettings.hpp"
#include <cstdlib>
#include <string>

namespace trading::settings {

class BrokerClientSettings : public IBrokerClientSettings {
public:
    std::string getHost() const override {
        const char* host = std::getenv("BROKER_SERVICE_HOST");
        return host ? host : "localhost";
    }
    
    int getPort() const override {
        const char* port = std::getenv("BROKER_SERVICE_PORT");
        return port ? std::stoi(port) : 8083;
    }
};

} // namespace trading::settings
