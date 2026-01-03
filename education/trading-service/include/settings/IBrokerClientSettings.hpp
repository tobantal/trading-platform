#pragma once

#include <string>

namespace trading::settings {

class IBrokerClientSettings {
public:
    virtual ~IBrokerClientSettings() = default;
    
    virtual std::string getHost() const = 0;
    virtual int getPort() const = 0;
};

} // namespace trading::settings

