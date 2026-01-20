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
     * @brief GET /api/v1/portfolio/positions — позиции портфеля
     *
     * Требует Access Token (содержит accountId).
     */
    class GetPositionsHandler : public IHttpHandler
    {
    public:
        GetPositionsHandler(
            std::shared_ptr<ports::input::IPortfolioService> portfolioService,
            std::shared_ptr<ports::output::IAuthClient> authClient) : portfolioService_(std::move(portfolioService)), authClient_(std::move(authClient))
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

            auto accountId = extractAccountId(req);
            if (!accountId)
            {
                sendError(res, 401, "Access token required. Use POST /api/v1/auth/select-account to get one.");
                return;
            }

            try
            {
                auto portfolio = portfolioService_->getPortfolio(*accountId);

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
        std::shared_ptr<ports::output::IAuthClient> authClient_;

        std::optional<std::string> extractAccountId(IRequest &req)
        {
            std::string token = req.getBearerToken().value_or("");
            return authClient_->getAccountIdFromToken(token);
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
