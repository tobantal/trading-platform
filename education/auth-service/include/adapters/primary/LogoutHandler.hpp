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
        auto token = req.getBearerToken().value_or("");
        if (token.empty()) {
            res.setResult(401, "application/json", R"({"error": "Authorization header required"})");
            return;
        }

        bool success = authService_->logout(token);
        
        nlohmann::json response;
        response["success"] = success;
        response["message"] = success ? "Logged out" : "Token not found";

        res.setResult(200, "application/json", response.dump());
    }

private:
    std::shared_ptr<ports::input::IAuthService> authService_;
};

} // namespace auth::adapters::primary
