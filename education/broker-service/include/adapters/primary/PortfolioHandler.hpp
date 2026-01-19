#pragma once

#include <IHttpHandler.hpp>
#include <IRequest.hpp>
#include <IResponse.hpp>
#include "ports/output/IBrokerGateway.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace broker::adapters::primary {

/**
 * @brief HTTP handler for portfolio endpoints
 * 
 * Endpoints:
 * - GET /api/v1/portfolio?account_id=...        - полный портфель
 * - GET /api/v1/portfolio/positions?account_id=... - только позиции
 * - GET /api/v1/portfolio/cash?account_id=...   - только кэш
 */
class PortfolioHandler : public IHttpHandler {
public:
    explicit PortfolioHandler(std::shared_ptr<ports::output::IBrokerGateway> broker)
        : broker_(std::move(broker))
    {
        std::cout << "[PortfolioHandler] Created" << std::endl;
    }

    void handle(IRequest& req, IResponse& res) override {
        std::string path = req.getPath();
        
        // Извлекаем account_id из query параметров
        auto params = req.getQueryParams();
        auto it = params.find("account_id");
        
        if (it == params.end() || it->second.empty()) {
            sendError(res, 400, "Parameter 'account_id' is required");
            return;
        }
        
        std::string accountId = it->second;
        
        try {
            // Роутинг по path
            if (path == "/api/v1/portfolio/positions") {
                handleGetPositions(res, accountId);
            } else if (path == "/api/v1/portfolio/cash") {
                handleGetCash(res, accountId);
            } else if (path == "/api/v1/portfolio") {
                handleGetPortfolio(res, accountId);
            } else {
                sendError(res, 404, "Not found");
            }
        } catch (const std::runtime_error& e) {
            // Account not found → 404
            sendError(res, 404, e.what());
        } catch (const std::exception& e) {
            std::cerr << "[PortfolioHandler] Error: " << e.what() << std::endl;
            sendError(res, 500, "Internal server error");
        }
    }

private:
    std::shared_ptr<ports::output::IBrokerGateway> broker_;

    /**
     * @brief GET /api/v1/portfolio - полный портфель
     */
    void handleGetPortfolio(IResponse& res, const std::string& accountId) {
        auto portfolio = broker_->getPortfolio(accountId);
        
        nlohmann::json response;
        response["account_id"] = accountId;
        
        // Cash - Postman ожидает units/nano/currency формат
        response["cash"] = {
            {"units", static_cast<int64_t>(portfolio.cash.toDouble())},
            {"nano", 0},
            {"currency", portfolio.cash.currency}
        };
        
        // Total value
        response["total_value"] = {
            {"units", static_cast<int64_t>(portfolio.totalValue.toDouble())},
            {"nano", 0},
            {"currency", portfolio.totalValue.currency}
        };
        
        // Positions
        response["positions"] = nlohmann::json::array();
        for (const auto& pos : portfolio.positions) {
            response["positions"].push_back(positionToJson(pos));
        }
        
        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

    /**
     * @brief GET /api/v1/portfolio/positions - только позиции
     * Postman ожидает массив напрямую
     */
    void handleGetPositions(IResponse& res, const std::string& accountId) {
        auto portfolio = broker_->getPortfolio(accountId);
        
        // Postman ожидает массив напрямую, не объект
        nlohmann::json response = nlohmann::json::array();
        
        for (const auto& pos : portfolio.positions) {
            response.push_back(positionToJson(pos));
        }
        
        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

    /**
     * @brief GET /api/v1/portfolio/cash - только кэш
     * Postman ожидает units/currency формат
     */
    void handleGetCash(IResponse& res, const std::string& accountId) {
        auto portfolio = broker_->getPortfolio(accountId);
        
        nlohmann::json response;
        response["units"] = static_cast<int64_t>(portfolio.cash.toDouble());
        response["nano"] = 0;
        response["currency"] = portfolio.cash.currency;
        
        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

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
        res.setStatus(status);
        res.setHeader("Content-Type", "application/json");
        res.setBody(error.dump());
    }
};

} // namespace broker::adapters::primary
