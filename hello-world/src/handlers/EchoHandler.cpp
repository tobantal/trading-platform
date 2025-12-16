
#include "handlers/EchoHandler.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

using json = nlohmann::json;

EchoHandler::EchoHandler()
{
    std::cout << "[EchoHandler] Created" << std::endl;
}

void EchoHandler::handle(IRequest& req, IResponse& res)
{
    std::cout << "[EchoHandler] Handling echo request" << std::endl;
    
    // Получаем параметр message из query string
    auto params = req.getParams();
    std::string message = "";
    
    auto it = params.find("message");
    if (it != params.end())
    {
        message = it->second;
    }
    
    res.setStatus(200);
    res.setHeader("Content-Type", "application/json");
    
    // Строим JSON ответ
    json response;
    response["message"] = message;
    response["timestamp"] = getCurrentTimestamp();
    
    res.setBody(response.dump());
    
    std::cout << "[EchoHandler] Response sent with message: " << message << std::endl;
}

std::string EchoHandler::getCurrentTimestamp() const
{
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}