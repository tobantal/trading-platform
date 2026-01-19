#pragma once


#include <IHttpHandler.hpp>
#include "ports/output/IBrokerGateway.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>


namespace broker::adapters::primary {


/**
 * @brief GET /api/v1/portfolio?account_id=xxx — полный портфель
 */
class GetPortfolioHandler : public IHttpHandler {
public:
    explicit GetPortfolioHandler(std::shared_ptr<ports::output::IBrokerGateway> broker)
        : broker_(std::move(broker))
    {
        std::cout << "[GetPortfolioHandler] Created" << std::endl;
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
            response["account_id"] = accountId;
            
            response["cash"] = {
                {"units", static_cast<int64_t>(portfolio.cash.toDouble())},
                {"nano", 0},
                {"currency", portfolio.cash.currency}
            };
            
            response["total_value"] = {
                {"units", static_cast<int64_t>(portfolio.totalValue.toDouble())},
                {"nano", 0},
                {"currency", portfolio.totalValue.currency}
            };
            
            response["positions"] = nlohmann::json::array();
            for (const auto& pos : portfolio.positions) {
                response["positions"].push_back(positionToJson(pos));
            }
            
            res.setResult(200, "application/json", response.dump());


        } catch (const std::runtime_error& e) {
            sendError(res, 404, e.what());
        } catch (const std::exception& e) {
            std::cerr << "[GetPortfolioHandler] Error: " << e.what() << std::endl;
            sendError(res, 500, "Internal server error");
        }
    }


private:
    std::shared_ptr<ports::output::IBrokerGateway> broker_;


    nlohmann::json positionToJson(const domain::Position& pos) {
        nlohmann::json j;
        j["figi"] = pos.figi;
        j["ticker"] = pos.ticker;
        j["quantity"] = pos.quantity;
        j["average_price"] = pos.averagePrice.toDouble();
        j["current_price"] = pos.currentPrice.toDouble();
        j["currency"] = pos.currentPrice.currency;
        j["pnl"] = pos.pnl.toDouble();
        j["pnl_percent"] = pos.pnlPercent;
        return j;
    }


    void sendError(IResponse& res, int status, const std::string& message) {
        nlohmann::json error;
        error["error"] = message;
        res.setResult(status, "application/json", error.dump());
    }
};


} // namespace broker::adapters::primary
