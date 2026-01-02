#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IAuthService.hpp"
#include <nlohmann/json.hpp>
#include <memory>

namespace auth::adapters::primary {

class LogoutHandler : public IHttpHandler {
public:
    explicit LogoutHandler(std::shared_ptr<ports::input::IAuthService> authService)
        : authService_(std::move(authService)) {}

    void handle(IRequest& req, IResponse& res) override {
        auto token = extractBearerToken(req);
        if (!token) {
            res.setStatus(401);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Authorization header required"})");
            return;
        }

        bool success = authService_->logout(*token);
        
        nlohmann::json response;
        response["success"] = success;
        response["message"] = success ? "Logged out" : "Token not found";

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

private:
    std::shared_ptr<ports::input::IAuthService> authService_;

    std::optional<std::string> extractBearerToken(IRequest& req) {
        auto headers = req.getHeaders();
        auto it = headers.find("Authorization");
        if (it == headers.end()) return std::nullopt;
        
        const std::string& auth = it->second;
        if (auth.substr(0, 7) != "Bearer ") return std::nullopt;
        return auth.substr(7);
    }
};

} // namespace auth::adapters::primary
