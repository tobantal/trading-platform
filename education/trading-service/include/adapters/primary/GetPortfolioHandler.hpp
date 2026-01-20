#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IPortfolioService.hpp"
#include "ports/output/IAuthClient.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary
{

    /**
     * @brief GET /api/v1/portfolio — полный портфель
     *
     * Требует Access Token (содержит accountId).
     */
    class GetPortfolioHandler : public IHttpHandler
    {
    public:
        GetPortfolioHandler(
            std::shared_ptr<ports::input::IPortfolioService> portfolioService,
            std::shared_ptr<ports::output::IAuthClient> authClient) : portfolioService_(std::move(portfolioService)), authClient_(std::move(authClient))
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

            auto accountId = extractAccountId(req);
            if (!accountId)
            {
                sendError(res, 401, "Access token required. Use POST /api/v1/auth/select-account to get one.");
                return;
            }

            try
            {
                auto portfolio = portfolioService_->getPortfolio(*accountId);

                nlohmann::json response;
                response["account_id"] = *accountId;
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
        std::shared_ptr<ports::output::IAuthClient> authClient_;

        std::optional<std::string> extractAccountId(IRequest &req)
        {
            std::string token = req.getBearerToken().value_or("");
            return authClient_->getAccountIdFromToken(token);
        }

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
