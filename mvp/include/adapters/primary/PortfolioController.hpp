#pragma once

#include "ports/input/IPortfolioService.hpp"
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include <http_server/IRequestHandler.hpp>
#include <nlohmann/json.hpp>
#include <memory>

namespace trading::adapters::primary {

/**
 * @brief REST контроллер портфеля
 * 
 * Endpoints:
 * - GET /api/v1/portfolio - получить портфель
 * - GET /api/v1/portfolio/positions - получить позиции
 * - GET /api/v1/portfolio/cash - получить доступные средства
 */
class PortfolioController : public microservice::IRequestHandler {
public:
    PortfolioController(
        std::shared_ptr<ports::input::IPortfolioService> portfolioService,
        std::shared_ptr<ports::input::IAuthService> authService,
        std::shared_ptr<ports::input::IAccountService> accountService
    ) : portfolioService_(std::move(portfolioService))
      , authService_(std::move(authService))
      , accountService_(std::move(accountService))
    {}

    microservice::Response handle(const microservice::Request& request) override {
        // Проверяем авторизацию
        auto userId = extractUserId(request);
        if (!userId) {
            return unauthorized();
        }

        // Получаем активный счёт
        auto account = accountService_->getActiveAccount(*userId);
        if (!account) {
            return badRequest("No active account found");
        }

        // GET /api/v1/portfolio
        if (request.method == "GET" && request.path == "/api/v1/portfolio") {
            return handleGetPortfolio(account->id);
        }
        
        // GET /api/v1/portfolio/positions
        if (request.method == "GET" && request.path == "/api/v1/portfolio/positions") {
            return handleGetPositions(account->id);
        }
        
        // GET /api/v1/portfolio/cash
        if (request.method == "GET" && request.path == "/api/v1/portfolio/cash") {
            return handleGetCash(account->id);
        }

        return notFound();
    }

private:
    std::shared_ptr<ports::input::IPortfolioService> portfolioService_;
    std::shared_ptr<ports::input::IAuthService> authService_;
    std::shared_ptr<ports::input::IAccountService> accountService_;

    microservice::Response handleGetPortfolio(const std::string& accountId) {
        auto portfolio = portfolioService_->getPortfolio(accountId);

        nlohmann::json response;
        response["account_id"] = accountId;
        response["cash"] = moneyToJson(portfolio.cash);
        response["total_value"] = moneyToJson(portfolio.totalValue);
        response["total_pnl"] = moneyToJson(portfolio.totalPnl);
        response["pnl_percent"] = portfolio.pnlPercent;

        nlohmann::json positions = nlohmann::json::array();
        for (const auto& pos : portfolio.positions) {
            positions.push_back(positionToJson(pos));
        }
        response["positions"] = positions;

        return jsonResponse(200, response);
    }

    microservice::Response handleGetPositions(const std::string& accountId) {
        auto positions = portfolioService_->getPositions(accountId);

        nlohmann::json response = nlohmann::json::array();
        for (const auto& pos : positions) {
            response.push_back(positionToJson(pos));
        }

        return jsonResponse(200, response);
    }

    microservice::Response handleGetCash(const std::string& accountId) {
        auto cash = portfolioService_->getAvailableCash(accountId);

        nlohmann::json response;
        response["available"] = cash.toDouble();
        response["currency"] = cash.currency;

        return jsonResponse(200, response);
    }

    nlohmann::json moneyToJson(const domain::Money& money) {
        nlohmann::json j;
        j["amount"] = money.toDouble();
        j["currency"] = money.currency;
        return j;
    }

    nlohmann::json positionToJson(const domain::Position& pos) {
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

    std::optional<std::string> extractUserId(const microservice::Request& request) {
        auto it = request.headers.find("Authorization");
        if (it == request.headers.end()) {
            return std::nullopt;
        }

        std::string auth = it->second;
        if (auth.find("Bearer ") != 0) {
            return std::nullopt;
        }

        std::string token = auth.substr(7);
        return authService_->getUserIdFromToken(token);
    }

    microservice::Response jsonResponse(int status, const nlohmann::json& body) {
        microservice::Response resp;
        resp.status = status;
        resp.headers["Content-Type"] = "application/json";
        resp.body = body.dump();
        return resp;
    }

    microservice::Response badRequest(const std::string& message) {
        nlohmann::json body;
        body["error"] = message;
        return jsonResponse(400, body);
    }

    microservice::Response unauthorized() {
        nlohmann::json body;
        body["error"] = "Unauthorized";
        return jsonResponse(401, body);
    }

    microservice::Response notFound() {
        nlohmann::json body;
        body["error"] = "Not found";
        return jsonResponse(404, body);
    }
};

} // namespace trading::adapters::primary
