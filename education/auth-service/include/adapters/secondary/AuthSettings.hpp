#pragma once

#include <string>
#include <cstdlib>

namespace auth::adapters::secondary {

/**
 * @brief Настройки Auth Service из ENV
 */
class AuthSettings {
public:
    AuthSettings() {
        sessionLifetimeSeconds_ = std::stoi(getEnvOrDefault("AUTH_SESSION_LIFETIME", "86400"));
    }

    int getSessionLifetimeSeconds() const { return sessionLifetimeSeconds_; }

private:
    int sessionLifetimeSeconds_;

    static std::string getEnvOrDefault(const char* name, const std::string& defaultValue) {
        const char* value = std::getenv(name);
        return value ? value : defaultValue;
    }
};

} // namespace auth::adapters::secondary
