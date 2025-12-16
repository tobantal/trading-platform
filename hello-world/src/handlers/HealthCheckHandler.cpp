#include "handlers/HealthCheckHandler.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

using json = nlohmann::json;

HealthCheckHandler::HealthCheckHandler()
{
    // Инициализируем список сервисов с их начальными статусами
    services_ = {
        {"http_server", "ready"},
        {"cache", "ready"},
        {"postgres", "pending"}
    };
    
    std::cout << "[HealthCheckHandler] Created" << std::endl;
}

void HealthCheckHandler::handle(IRequest& req, IResponse& res)
{
    std::cout << "[HealthCheckHandler] Handling request from " << req.getIp() << std::endl;
    
    // Устанавливаем статус ответа
    res.setStatus(200);
    res.setHeader("Content-Type", "application/json");
    
    // Устанавливаем тело ответа
    res.setBody(buildHealthJson());
    
    std::cout << "[HealthCheckHandler] Response sent" << std::endl;
}

std::string HealthCheckHandler::buildHealthJson() const
{
    json response;
    response["status"] = "ok";
    
    // Добавляем timestamp
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    response["timestamp"] = oss.str();
    
    // Добавляем статусы сервисов
    json services;
    for (const auto& service : services_)
    {
        services[service.name] = service.status;
    }
    response["services"] = services;
    
    return response.dump();
}