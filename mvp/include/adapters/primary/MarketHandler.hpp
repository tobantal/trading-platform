#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IMarketService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>
#include <sstream>

namespace trading::adapters::primary {

/**
 * @brief HTTP Handler для рыночных данных
 * 
 * Endpoints:
 * - GET /api/v1/quotes
 * - GET /api/v1/instruments
 * - GET /api/v1/instruments/search
 * - GET /api/v1/instruments/{figi}
 */
class MarketHandler : public IHttpHandler
{
public:
    explicit MarketHandler(std::shared_ptr<ports::input::IMarketService> marketService)
        : marketService_(std::move(marketService))
    {
        std::cout << "[MarketHandler] Created" << std::endl;
    }

    void handle(IRequest& req, IResponse& res) override
    {
        std::string path = req.getPath();
        
        if (path == "/api/v1/quotes") {
            handleGetQuotes(req, res);
        } else if (path == "/api/v1/instruments") {
            handleGetInstruments(req, res);
        } else if (path == "/api/v1/instruments/search") {
            handleSearchInstruments(req, res);
        } else if (path.find("/api/v1/instruments/") == 0) {
            handleGetInstrumentByFigi(req, res);
        } else {
            res.setStatus(404);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Not found"})");
        }
    }

private:
    std::shared_ptr<ports::input::IMarketService> marketService_;

    void handleGetQuotes(IRequest& req, IResponse& res)
    {
        auto params = req.getParams();
        std::vector<std::string> figis;
        
        auto it = params.find("figis");
        if (it != params.end() && !it->second.empty()) {
            // Парсим figis через запятую
            std::stringstream ss(it->second);
            std::string item;
            while (std::getline(ss, item, ',')) {
                if (!item.empty()) {
                    figis.push_back(item);
                }
            }
        }
        
        if (figis.empty()) {
            // Если не указаны - возвращаем все инструменты
            auto instruments = marketService_->getAllInstruments();
            for (const auto& instr : instruments) {
                figis.push_back(instr.figi);
            }
        }

        auto quotes = marketService_->getQuotes(figis);

        nlohmann::json response = nlohmann::json::array();
        for (const auto& quote : quotes) {
            response.push_back(quoteToJson(quote));
        }

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

    void handleGetInstruments(IRequest& req, IResponse& res)
    {
        auto instruments = marketService_->getAllInstruments();

        nlohmann::json response = nlohmann::json::array();
        for (const auto& instr : instruments) {
            response.push_back(instrumentToJson(instr));
        }

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

    void handleSearchInstruments(IRequest& req, IResponse& res)
    {
        auto params = req.getParams();
        std::string query;
        
        auto it = params.find("query");
        if (it != params.end()) {
            query = it->second;
        }

        if (query.empty()) {
            res.setStatus(400);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Query parameter is required"})");
            return;
        }

        auto instruments = marketService_->searchInstruments(query);

        nlohmann::json response = nlohmann::json::array();
        for (const auto& instr : instruments) {
            response.push_back(instrumentToJson(instr));
        }

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

    void handleGetInstrumentByFigi(IRequest& req, IResponse& res)
    {
        std::string path = req.getPath();
        std::string figi = path.substr(std::string("/api/v1/instruments/").length());
        
        if (figi.empty()) {
            res.setStatus(400);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "FIGI is required"})");
            return;
        }

        auto instrument = marketService_->getInstrumentByFigi(figi);
        if (!instrument) {
            res.setStatus(404);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Instrument not found"})");
            return;
        }

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(instrumentToJson(*instrument).dump());
    }

    nlohmann::json quoteToJson(const domain::Quote& quote)
    {
        nlohmann::json j;
        j["figi"] = quote.figi;
        j["ticker"] = quote.ticker;
        j["last_price"] = quote.lastPrice.toDouble();
        j["bid_price"] = quote.bidPrice.toDouble();
        j["ask_price"] = quote.askPrice.toDouble();
        j["currency"] = quote.lastPrice.currency;
        j["updated_at"] = quote.updatedAt.toString();
        return j;
    }

    nlohmann::json instrumentToJson(const domain::Instrument& instr)
    {
        nlohmann::json j;
        j["figi"] = instr.figi;
        j["ticker"] = instr.ticker;
        j["name"] = instr.name;
        j["currency"] = instr.currency;
        j["lot"] = instr.lot;
        j["trading_available"] = instr.tradingAvailable;
        return j;
    }
};

} // namespace trading::adapters::primary
