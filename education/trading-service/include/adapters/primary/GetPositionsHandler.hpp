#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IPortfolioService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary
{

    class GetPositionsHandler : public IHttpHandler
    {
    public:
        explicit GetPositionsHandler(
            std::shared_ptr<ports::input::IPortfolioService> portfolioService)
            : portfolioService_(std::move(portfolioService))
        {
            std::cout << "[GetPositionsHandler] Created" << std::endl;
        }

        void handle(IRequest &req, IResponse &res) override
        {
            if (req.getMethod() != "GET")
            {
                sendError(res, 405, "Method not allowed");
                return;
            }

            auto accountId = req.getAttribute("accountId").value_or("");
            if (accountId.empty())
            {
                sendError(res, 500, "Internal server error");
                std::cout << "[GetPositionsHandler] Error: accountId must not be null on this step." << std::endl;
                return;
            }

            try
            {
                auto portfolio = portfolioService_->getPortfolio(accountId);

                nlohmann::json response = nlohmann::json::array();
                for (const auto &pos : portfolio.positions)
                {
                    response.push_back(positionToJson(pos));
                }

                res.setResult(200, "application/json", response.dump());
            }
            catch (const std::exception &e)
            {
                std::cerr << "[GetPositionsHandler] Error: " << e.what() << std::endl;
                sendError(res, 500, "Internal server error");
            }
        }

    private:
        std::shared_ptr<ports::input::IPortfolioService> portfolioService_;

        nlohmann::json positionToJson(const domain::Position &pos)
        {
            nlohmann::json j;
            j["figi"] = pos.figi;
            j["ticker"] = pos.ticker;
            j["quantity"] = pos.quantity;
            j["average_price"] = pos.averagePrice.toDouble();
            j["current_price"] = pos.currentPrice.toDouble();
            j["currency"] = pos.averagePrice.currency;
            j["pnl"] = pos.pnl.toDouble();
            j["pnl_percent"] = pos.pnlPercent;
            return j;
        }

        void sendError(IResponse &res, int status, const std::string &message)
        {
            nlohmann::json error;
            error["error"] = message;
            res.setResult(status, "application/json", error.dump());
        }
    };

} // namespace trading::adapters::primary
