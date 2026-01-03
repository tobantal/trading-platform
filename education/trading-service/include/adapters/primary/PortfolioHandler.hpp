#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IPortfolioService.hpp"
#include "ports/output/IAuthClient.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief HTTP Handler для портфеля
 * 
 * Endpoints:
 * - GET /api/v1/portfolio           → полный портфель
 * - GET /api/v1/portfolio/positions → позиции
 * - GET /api/v1/portfolio/cash      → доступные средства
 * 
 * Требует Access Token (содержит accountId).
 */
class PortfolioHandler : public IHttpHandler
{
public:
    PortfolioHandler(
        std::shared_ptr<ports::input::IPortfolioService> portfolioService,
        std::shared_ptr<ports::output::IAuthClient> authClient
    ) : portfolioService_(std::move(portfolioService))
      , authClient_(std::move(authClient))
    {
        std::cout << "[PortfolioHandler] Created" << std::endl;
    }

    void handle(IRequest& req, IResponse& res) override
    {
        // Извлекаем accountId из access token
        auto accountId = extractAccountId(req);
        if (!accountId) {
            res.setStatus(401);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Access token required. Use POST /api/v1/auth/select-account to get one."})");
            return;
        }

        std::string path = req.getPath();
        
        // Убираем query string для маршрутизации
        size_t queryPos = path.find('?');
        if (queryPos != std::string::npos) {
            path = path.substr(0, queryPos);
        }
        
        if (path == "/api/v1/portfolio") {
            handleGetPortfolio(req, res, *accountId);
        } else if (path == "/api/v1/portfolio/positions") {
            handleGetPositions(req, res, *accountId);
        } else if (path == "/api/v1/portfolio/cash") {
            handleGetCash(req, res, *accountId);
        } else {
            res.setStatus(404);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Not found"})");
        }
    }

private:
    std::shared_ptr<ports::input::IPortfolioService> portfolioService_;
    std::shared_ptr<ports::output::IAuthClient> authClient_;

    /**
     * @brief Обрабатывает запрос получения полного портфеля.
     */
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

    /**
     * @brief Обрабатывает запрос получения позиций портфеля.
     */
    void handleGetPositions(IRequest& req, IResponse& res, const std::string& accountId)
    {
        auto portfolio = portfolioService_->getPortfolio(accountId);

        nlohmann::json response = nlohmann::json::array();
        for (const auto& pos : portfolio.positions) {
            response.push_back(positionToJson(pos));
        }

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

    /**
     * @brief Обрабатывает запрос получения доступных денежных средств.
     */
    void handleGetCash(IRequest& req, IResponse& res, const std::string& accountId)
    {
        auto cash = portfolioService_->getAvailableCash(accountId);

        nlohmann::json response;
        response["amount"] = cash.toDouble();
        response["currency"] = cash.currency;

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

    /**
     * @brief Преобразует объект Money в JSON.
     */
    nlohmann::json moneyToJson(const domain::Money& money)
    {
        nlohmann::json j;
        j["amount"] = money.toDouble();
        j["currency"] = money.currency;
        return j;
    }

    /**
     * @brief Преобразует объект Position в JSON.
     */
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

    /**
     * @brief Извлекает accountId из access token через auth-service
     */
    std::optional<std::string> extractAccountId(IRequest& req)
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
        return authClient_->getAccountIdFromToken(token);
    }
};

} // namespace trading::adapters::primary
