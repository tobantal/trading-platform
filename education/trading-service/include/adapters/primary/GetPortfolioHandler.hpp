#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IPortfolioService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary
{

    class GetPortfolioHandler : public IHttpHandler
    {
    public:
        explicit GetPortfolioHandler(
            std::shared_ptr<ports::input::IPortfolioService> portfolioService)
            : portfolioService_(std::move(portfolioService))
        {
            std::cout << "[GetPortfolioHandler] Created" << std::endl;
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
                std::cout << "[GetPortfolioHandler] Error: accountId must not be null on this step." << std::endl;
                return;
            }

            try
            {
                auto portfolio = portfolioService_->getPortfolio(accountId);

                nlohmann::json response;
                response["account_id"] = accountId;
                response["cash"] = moneyToJson(portfolio.cash);
                response["total_value"] = moneyToJson(portfolio.totalValue);
                response["total_pnl"] = moneyToJson(portfolio.totalPnl());

                double pnlPercent = 0.0;
                if (portfolio.totalValue.toDouble() > 0)
                {
                    double costBasis = portfolio.totalValue.toDouble() - portfolio.totalPnl().toDouble();
                    if (costBasis > 0)
                    {
                        pnlPercent = (portfolio.totalPnl().toDouble() / costBasis) * 100.0;
                    }
                }
                response["pnl_percent"] = pnlPercent;

                nlohmann::json positions = nlohmann::json::array();
                for (const auto &pos : portfolio.positions)
                {
                    positions.push_back(positionToJson(pos));
                }
                response["positions"] = positions;

                res.setResult(200, "application/json", response.dump());
            }
            catch (const std::exception &e)
            {
                std::cerr << "[GetPortfolioHandler] Error: " << e.what() << std::endl;
                sendError(res, 500, "Internal server error");
            }
        }

    private:
        std::shared_ptr<ports::input::IPortfolioService> portfolioService_;

        nlohmann::json moneyToJson(const domain::Money &money)
        {
            nlohmann::json j;
            j["amount"] = money.toDouble();
            j["currency"] = money.currency;
            return j;
        }

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
