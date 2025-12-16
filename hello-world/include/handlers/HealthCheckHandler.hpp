#pragma once

#include <IHttpHandler.hpp>
#include <string>
#include <vector>

/**
 * @class HealthCheckHandler
 * @brief Обработчик для проверки живости сервера
 * 
 * Endpoint: GET /api/v1/health
 * 
 * Response (200 OK):
 * {
 *   "status": "ok",
 *   "timestamp": "2025-12-14T15:30:45Z",
 *   "services": {
 *     "http_server": "ready",
 *     "cache": "ready",
 *     "postgres": "pending"
 *   }
 * }
 */
class HealthCheckHandler : public IHttpHandler
{
public:
    HealthCheckHandler();
    virtual ~HealthCheckHandler() = default;

    /**
     * @brief Обработать HTTP запрос проверки здоровья сервера
     */
    void handle(IRequest& req, IResponse& res) override;

private:
    struct ServiceStatus
    {
        std::string name;      ///< Название сервиса
        std::string status;    ///< "ready", "pending", "error"
    };

    std::vector<ServiceStatus> services_;

    /**
     * @brief Построить JSON ответ о состоянии сервера
     */
    std::string buildHealthJson() const;
};