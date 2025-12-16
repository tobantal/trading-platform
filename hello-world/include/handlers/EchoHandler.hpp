#pragma once

#include "IHttpHandler.hpp"
#include <string>

/**
 * @class EchoHandler
 * @brief Простой обработчик для echo запросов
 * 
 * Endpoint: GET /echo?message=hello
 * 
 * Response (200 OK):
 * {
 *   "message": "hello",
 *   "timestamp": "2025-12-14T15:30:45Z"
 * }
 * 
 * Если параметр message отсутствует:
 * {
 *   "message": "",
 *   "timestamp": "2025-12-14T15:30:45Z"
 * }
 */
class EchoHandler : public IHttpHandler
{
public:
    EchoHandler();
    virtual ~EchoHandler() = default;

    void handle(IRequest& req, IResponse& res) override;

private:
    /**
     * @brief Получить текущее время в ISO 8601 формате
     */
    std::string getCurrentTimestamp() const;
};