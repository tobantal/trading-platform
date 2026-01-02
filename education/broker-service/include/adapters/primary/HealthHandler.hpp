#pragma once

#include <IHttpHandler.hpp>
#include <IRequest.hpp>
#include <IResponse.hpp>
#include <nlohmann/json.hpp>

namespace broker::adapters::primary {

class HealthHandler : public IHttpHandler {
public:
    void handle(IRequest& req, IResponse& res) override {
        nlohmann::json response;
        response["status"] = "healthy";
        response["service"] = "broker-service";
        response["version"] = "1.0.0";

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }
};

} // namespace broker::adapters::primary
