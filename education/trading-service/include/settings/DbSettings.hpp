// include/settings/DbSettings.hpp
#pragma once

#include <string>
#include <cstdlib>
#include <stdexcept>

namespace trading::settings
{

    /**
     * @brief Настройки подключения к PostgreSQL
     *
     * Читает параметры из переменных окружения (K8s ENV).
     */
    class DbSettings
    {
    public:
        DbSettings()
        {
            host_ = getEnvOrDefault("TRADING_DB_HOST", "trading-postgres");
            port_ = std::stoi(getEnvOrDefault("TRADING_DB_PORT", "5432"));
            name_ = getEnvOrDefault("TRADING_DB_NAME", "trading_db");
            user_ = getEnvOrDefault("TRADING_DB_USER", "trading_user");
            password_ = getEnvOrDefault("TRADING_DB_PASSWORD", "trading_secret_password");
        }

        std::string getHost() const { return host_; }
        int getPort() const { return port_; }
        std::string getName() const { return name_; }
        std::string getUser() const { return user_; }
        std::string getPassword() const { return password_; }

        std::string getConnectionString() const
        {
            return "host=" + host_ + " port=" + std::to_string(port_) +
                   " dbname=" + name_ + " user=" + user_ + " password=" + password_;
        }

    private:
        std::string host_;
        int port_;
        std::string name_;
        std::string user_;
        std::string password_;

        static std::string getEnvOrDefault(const char *name, const char *defaultValue)
        {
            const char *value = std::getenv(name);
            return value ? std::string(value) : std::string(defaultValue);
        }
    };

} // namespace trading::settings
