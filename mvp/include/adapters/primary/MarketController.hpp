#pragma once

#include "ports/input/IMarketService.hpp"
#include <http_server/IRequestHandler.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <sstream>
#include <algorithm>

namespace trading::adapters::primary {

/**
 * @brief REST контроллер рыночных данных
 * 
 * Endpoints:
 * - GET /api/v1/quotes?figis=... - получить котировки
 * - GET /api/v1/instruments - список всех инструментов
 * - GET /api/v1/instruments/search?query=... - поиск инструментов
 * - GET /api/v1/instruments/{figi} - информация об инструменте
 */
class MarketController : public microservice::IRequestHandler {
public:
    explicit MarketController(std::shared_ptr<ports::input::IMarketService> marketService)
        : marketService_(std::move(marketService))
    {}

    microservice::Response handle(const microservice::Request& request) override {
        // GET /api/v1/quotes
        if (request.method == "GET" && request.path == "/api/v1/quotes") {
            return handleGetQuotes(request);
        }
        
        // GET /api/v1/instruments
        if (request.method == "GET" && request.path == "/api/v1/instruments") {
            return handleGetInstruments(request);
        }
        
        // GET /api/v1/instruments/search
        if (request.method == "GET" && request.path == "/api/v1/instruments/search") {
            return handleSearchInstruments(request);
        }
        
        // GET /api/v1/instruments/{figi}
        if (request.method == "GET" && request.path.find("/api/v1/instruments/") == 0) {
            return handleGetInstrumentByFigi(request);
        }

        return notFound();
    }

private:
    std::shared_ptr<ports::input::IMarketService> marketService_;

    microservice::Response handleGetQuotes(const microservice::Request& request) {
        // Парсим query параметр figis
        auto figis = parseQueryParam(request.queryString, "figis");
        
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

        return jsonResponse(200, response);
    }

    microservice::Response handleGetInstruments(const microservice::Request& request) {
        auto instruments = marketService_->getAllInstruments();

        nlohmann::json response = nlohmann::json::array();
        for (const auto& instr : instruments) {
            response.push_back(instrumentToJson(instr));
        }

        return jsonResponse(200, response);
    }

    microservice::Response handleSearchInstruments(const microservice::Request& request) {
        auto queryParams = parseQueryParam(request.queryString, "query");
        std::string query = queryParams.empty() ? "" : queryParams[0];

        if (query.empty()) {
            return badRequest("Query parameter is required");
        }

        auto instruments = marketService_->searchInstruments(query);

        nlohmann::json response = nlohmann::json::array();
        for (const auto& instr : instruments) {
            response.push_back(instrumentToJson(instr));
        }

        return jsonResponse(200, response);
    }

    microservice::Response handleGetInstrumentByFigi(const microservice::Request& request) {
        // Извлекаем figi из пути /api/v1/instruments/{figi}
        std::string figi = request.path.substr(std::string("/api/v1/instruments/").length());
        
        if (figi.empty()) {
            return badRequest("FIGI is required");
        }

        auto instrument = marketService_->getInstrumentByFigi(figi);
        if (!instrument) {
            return notFound("Instrument not found: " + figi);
        }

        return jsonResponse(200, instrumentToJson(*instrument));
    }

    nlohmann::json quoteToJson(const domain::Quote& quote) {
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

    nlohmann::json instrumentToJson(const domain::Instrument& instr) {
        nlohmann::json j;
        j["figi"] = instr.figi;
        j["ticker"] = instr.ticker;
        j["name"] = instr.name;
        j["currency"] = instr.currency;
        j["lot"] = instr.lot;
        j["trading_available"] = instr.tradingAvailable;
        return j;
    }

    std::vector<std::string> parseQueryParam(const std::string& queryString, const std::string& param) {
        std::vector<std::string> result;
        
        // Простой парсинг query string
        std::string search = param + "=";
        size_t pos = queryString.find(search);
        if (pos == std::string::npos) {
            return result;
        }

        size_t start = pos + search.length();
        size_t end = queryString.find('&', start);
        std::string value = (end == std::string::npos) 
            ? queryString.substr(start) 
            : queryString.substr(start, end - start);

        // Разбиваем по запятой
        std::stringstream ss(value);
        std::string item;
        while (std::getline(ss, item, ',')) {
            if (!item.empty()) {
                result.push_back(item);
            }
        }

        return result;
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

    microservice::Response notFound(const std::string& message = "Not found") {
        nlohmann::json body;
        body["error"] = message;
        return jsonResponse(404, body);
    }
};

} // namespace trading::adapters::primary
