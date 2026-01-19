#pragma once

#include <IHttpHandler.hpp>
#include "ports/output/IBrokerGateway.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace broker::adapters::primary {

/**
 * @brief GET /api/v1/portfolio/cash?account_id=xxx — только кэш
 */
class GetCashHandler : public IHttpHandler {
public:
    explicit GetCashHandler(std::shared_ptr<ports::output::IBrokerGateway> broker)
        : broker_(std::move(broker))
    {
        std::cout << "[GetCashHandler] Created" << std::endl;
    }

    void handle(IRequest& req, IResponse& res) override {
        if (req.getMethod() != "GET") {
            sendError(res, 405, "Method not allowed");
            return;
        }

        try {
            std::string accountId = req.getQueryParam("account_id").value_or("");
            
            if (accountId.empty()) {
                sendError(res, 400, "Parameter 'account_id' is required");
                return;
            }

            auto portfolio = broker_->getPortfolio(accountId);
            
            nlohmann::json response;
            response["units"] = static_cast<int64_t>(portfolio.cash.toDouble());
            response["nano"] = 0;
            response["currency"] = portfolio.cash.currency;
            
            res.setResult(200, "application/json", response.dump());

        } catch (const std::runtime_error& e) {
            sendError(res, 404, e.what());
        } catch (const std::exception& e) {
            std::cerr << "[GetCashHandler] Error: " << e.what() << std::endl;
            sendError(res, 500, "Internal server error");
        }
    }

private:
    std::shared_ptr<ports::output::IBrokerGateway> broker_;

    void sendError(IResponse& res, int status, const std::string& message) {
        nlohmann::json error;
        error["error"] = message;
        res.setResult(status, "application/json", error.dump());
    }
};

} // namespace broker::adapters::primary
