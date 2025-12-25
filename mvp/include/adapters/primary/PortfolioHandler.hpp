#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IPortfolioService.hpp"
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief HTTP Handler для портфеля
 * 
 * Endpoints:
 * - GET /api/v1/portfolio
 * - GET /api/v1/portfolio/positions
 * - GET /api/v1/portfolio/cash
 */
class PortfolioHandler : public IHttpHandler
{
public:
    PortfolioHandler(
        std::shared_ptr<ports::input::IPortfolioService> portfolioService,
        std::shared_ptr<ports::input::IAuthService> authService,
        std::shared_ptr<ports::input::IAccountService> accountService
    ) : portfolioService_(std::move(portfolioService))
      , authService_(std::move(authService))
      , accountService_(std::move(accountService))
    {
        std::cout << "[PortfolioHandler] Created" << std::endl;
    }

    void handle(IRequest& req, IResponse& res) override
    {
        // Проверяем авторизацию
        auto userId = extractUserId(req);
        if (!userId) {
            res.setStatus(401);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Unauthorized"})");
            return;
        }

        // Получаем активный счёт
        auto account = accountService_->getActiveAccount(*userId);
        if (!account) {
            res.setStatus(400);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "No active account found"})");
            return;
        }

        std::string path = req.getPath();
        
        if (path == "/api/v1/portfolio") {
            handleGetPortfolio(req, res, account->id);
        } else if (path == "/api/v1/portfolio/positions") {
            handleGetPositions(req, res, account->id);
        } else if (path == "/api/v1/portfolio/cash") {
            handleGetCash(req, res, account->id);
        } else {
            res.setStatus(404);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Not found"})");
        }
    }

private:
    std::shared_ptr<ports::input::IPortfolioService> portfolioService_;
    std::shared_ptr<ports::input::IAuthService> authService_;
    std::shared_ptr<ports::input::IAccountService> accountService_;

    void handleGetPortfolio(IRequest& req, IResponse& res, const std::string& accountId)
    {
        auto portfolio = portfolioService_->getPortfolio(accountId);

        nlohmann::json response;
        response["account_id"] = accountId;
        response["cash"] = moneyToJson(portfolio.cash);
        response["total_value"] = moneyToJson(portfolio.totalValue);
        response["total_pnl"] = moneyToJson(portfolio.totalPnl());
        
        // Рассчитываем pnlPercent
        double pnlPercent = 0.0;
        if (portfolio.totalValue.toDouble() > 0) {
            double costBasis = portfolio.totalValue.toDouble() - portfolio.totalPnl().toDouble();
            if (costBasis > 0) {
                pnlPercent = (portfolio.totalPnl().toDouble() / costBasis) * 100.0;
            }
        }
        response["pnl_percent"] = pnlPercent;

        nlohmann::json positions = nlohmann::json::array();
        for (const auto& pos : portfolio.positions) {
            positions.push_back(positionToJson(pos));
        }
        response["positions"] = positions;

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

    void handleGetPositions(IRequest& req, IResponse& res, const std::string& accountId)
    {
        // Получаем позиции через portfolio
        auto portfolio = portfolioService_->getPortfolio(accountId);

        nlohmann::json response = nlohmann::json::array();
        for (const auto& pos : portfolio.positions) {
            response.push_back(positionToJson(pos));
        }

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

    void handleGetCash(IRequest& req, IResponse& res, const std::string& accountId)
    {
        auto cash = portfolioService_->getAvailableCash(accountId);

        nlohmann::json response;
        response["available"] = cash.toDouble();
        response["currency"] = cash.currency;

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

    nlohmann::json moneyToJson(const domain::Money& money)
    {
        nlohmann::json j;
        j["amount"] = money.toDouble();
        j["currency"] = money.currency;
        return j;
    }

    nlohmann::json positionToJson(const domain::Position& pos)
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

    std::optional<std::string> extractUserId(IRequest& req)
    {
        auto headers = req.getHeaders();
        auto it = headers.find("Authorization");
        if (it == headers.end()) {
            return std::nullopt;
        }

        std::string auth = it->second;
        if (auth.find("Bearer ") != 0) {
            return std::nullopt;
        }

        std::string token = auth.substr(7);
        return authService_->getUserIdFromToken(token);
    }
};

} // namespace trading::adapters::primary
