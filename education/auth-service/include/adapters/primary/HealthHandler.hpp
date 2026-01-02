#pragma once

#include <IHttpHandler.hpp>
#include <nlohmann/json.hpp>

namespace auth::adapters::primary {

/**
 * @brief Health check handler
 * 
 * GET /health
 */
class HealthHandler : public IHttpHandler {
public:
    void handle(IRequest& req, IResponse& res) override {
        nlohmann::json response;
        response["status"] = "healthy";
        response["service"] = "auth-service";
        response["version"] = "1.0.0";

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }
};

} // namespace auth::adapters::primary
