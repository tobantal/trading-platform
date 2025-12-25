#pragma once

#include <IHttpHandler.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief HTTP Handler для проверки здоровья сервера
 * 
 * Endpoint: GET /api/v1/health
 */
class HealthHandler : public IHttpHandler
{
public:
    HealthHandler()
    {
        std::cout << "[HealthHandler] Created" << std::endl;
    }

    void handle(IRequest& req, IResponse& res) override
    {
        nlohmann::json response;
        response["status"] = "ok";
        response["timestamp"] = getCurrentTimestamp();
        
        nlohmann::json services;
        services["http_server"] = "ready";
        services["fake_tinkoff"] = "ready";
        services["fake_jwt"] = "ready";
        services["lru_cache"] = "ready";
        services["event_bus"] = "ready";
        services["postgres"] = "pending"; // TODO: В Education заменить на реальную проверку
        
        response["services"] = services;
        
        response["version"] = "1.0.0";
        response["architecture"] = "hexagonal";

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump(2)); // Pretty print
    }

private:
    std::string getCurrentTimestamp() const
    {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::gmtime(&t);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }
};

} // namespace trading::adapters::primary
