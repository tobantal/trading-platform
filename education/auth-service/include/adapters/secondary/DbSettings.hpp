#pragma once

#include <string>
#include <cstdlib>
#include <stdexcept>

namespace auth::adapters::secondary {

/**
 * @brief Настройки подключения к БД из ENV
 */
class DbSettings {
public:
    DbSettings() {
        host_ = getEnvOrDefault("AUTH_DB_HOST", "localhost");
        port_ = std::stoi(getEnvOrDefault("AUTH_DB_PORT", "5432"));
        name_ = getEnvOrDefault("AUTH_DB_NAME", "auth_db");
        user_ = getEnvOrDefault("AUTH_DB_USER", "auth_user");
        password_ = getEnvOrThrow("AUTH_DB_PASSWORD");
    }

    std::string getHost() const { return host_; }
    int getPort() const { return port_; }
    std::string getName() const { return name_; }
    std::string getUser() const { return user_; }
    std::string getPassword() const { return password_; }

    std::string getConnectionString() const {
        return "host=" + host_ + 
               " port=" + std::to_string(port_) +
               " dbname=" + name_ +
               " user=" + user_ +
               " password=" + password_;
    }

private:
    std::string host_;
    int port_;
    std::string name_;
    std::string user_;
    std::string password_;

    static std::string getEnvOrDefault(const char* name, const std::string& defaultValue) {
        const char* value = std::getenv(name);
        return value ? value : defaultValue;
    }

    static std::string getEnvOrThrow(const char* name) {
        const char* value = std::getenv(name);
        if (!value) {
            throw std::runtime_error(std::string("Required env variable not set: ") + name);
        }
        return value;
    }
};

} // namespace auth::adapters::secondary
