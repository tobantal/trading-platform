#pragma once

#include <IHttpHandler.hpp>
#include <IRequest.hpp>
#include <IResponse.hpp>
#include <nlohmann/json.hpp>

namespace trading::adapters::primary {

class HealthHandler : public IHttpHandler {
public:
    void handle(IRequest& req, IResponse& res) override {
        nlohmann::json response;
        response["status"] = "healthy";
        response["service"] = "trading-service";
        response["version"] = "1.0.0";

        res.setResult(200, "application/json", response.dump());
    }
};

} // namespace trading::adapters::primary
