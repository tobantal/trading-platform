#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IMarketService.hpp"

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>

namespace trading::adapters::primary {

class MarketHandler final : public IHttpHandler
{
public:
    explicit MarketHandler(std::shared_ptr<ports::input::IMarketService> marketService)
        : marketService_(std::move(marketService))
    {
    }

    void handle(IRequest& req, IResponse& res) override
    {
        if (req.getMethod() != "GET") {
            methodNotAllowed(res);
            return;
        }

        const std::string& fullPath = req.getPath();

        std::cout << "[MarketService] req.getPath()=" << fullPath << std::endl;

        const std::string path = extractPath(fullPath);

        if (path == "/api/v1/quotes") {
            handleGetQuotes(req, res);
            return;
        }

        if (path == "/api/v1/instruments") {
            handleGetInstruments(req, res);
            return;
        }

        if (path == "/api/v1/instruments/search") {
            handleSearchInstruments(req, res);
            return;
        }

        if (isInstrumentByFigiPath(path)) {
            handleGetInstrumentByFigi(path, res);
            return;
        }

        notFound(res);
    }

private:
    std::shared_ptr<ports::input::IMarketService> marketService_;

private:
    /**
     * @brief Извлекает базовый путь без query параметров
     */
    static std::string extractPath(const std::string& fullPath)
    {
        auto pos = fullPath.find('?');
        return pos == std::string::npos ? fullPath : fullPath.substr(0, pos);
    }

    /**
     * @brief Парсит query параметры из полного URL
     * 
     * Необходимо для совместимости с SimpleRequest в тестах,
     * так как SimpleRequest.getParams() всегда возвращает пустую map.
     * В production коде BeastRequestAdapter корректно парсит параметры.
     */
    static std::map<std::string, std::string> parseQueryParams(const std::string& fullPath)
    {
        std::map<std::string, std::string> params;
        
        auto pos = fullPath.find('?');
        if (pos == std::string::npos) {
            return params;
        }
        
        std::string query = fullPath.substr(pos + 1);
        size_t start = 0;
        
        while (start < query.size()) {
            auto eq = query.find('=', start);
            auto amp = query.find('&', start);
            
            if (eq == std::string::npos) {
                break;
            }
            
            std::string key = query.substr(start, eq - start);
            std::string value = (amp == std::string::npos)
                ? query.substr(eq + 1)
                : query.substr(eq + 1, amp - eq - 1);
            
            if (!key.empty()) {
                params[key] = value;
            }
            
            if (amp == std::string::npos) {
                break;
            }
            start = amp + 1;
        }
        
        return params;
    }

    /**
     * @brief Получает query параметры
     * 
     * Приоритет:
     * 1. Парсим из URL (надёжно работает для всех реализаций IRequest)
     * 2. Fallback на req.getParams() (если URL не содержит параметров)
     */
    static std::map<std::string, std::string> getParams(IRequest& req)
    {
        // Сначала парсим из URL - это работает для ВСЕХ реализаций IRequest
        auto params = parseQueryParams(req.getPath());
        
        // Fallback на getParams() если в URL параметров нет
        if (params.empty()) {
            params = req.getParams();
        }
        
        return params;
    }

    static bool isInstrumentByFigiPath(const std::string& path)
    {
        constexpr const char* Prefix = "/api/v1/instruments/";
        const size_t prefixLen = std::strlen(Prefix);
        
        // Путь должен начинаться с prefix и быть >= его длины
        // (включая случай "/api/v1/instruments/" без FIGI)
        return path.size() >= prefixLen &&
               path.compare(0, prefixLen, Prefix) == 0;
    }

    /* ===================== handlers ===================== */

    void handleGetQuotes(IRequest& req, IResponse& res)
    {
        const auto params = getParams(req);
        std::vector<std::string> figis;

        if (auto it = params.find("figis"); it != params.end()) {
            std::stringstream ss(it->second);
            std::string figi;
            while (std::getline(ss, figi, ',')) {
                if (!figi.empty()) {
                    figis.push_back(figi);
                }
            }
        }

        if (figis.empty()) {
            for (const auto& instr : marketService_->getAllInstruments()) {
                figis.push_back(instr.figi);
            }
        }

        const auto quotes = marketService_->getQuotes(figis);

        nlohmann::json response = nlohmann::json::array();
        for (const auto& q : quotes) {
            response.push_back(quoteToJson(q));
        }

        okJson(res, response);
    }

    void handleGetInstruments(IRequest&, IResponse& res)
    {
        const auto instruments = marketService_->getAllInstruments();

        nlohmann::json response = nlohmann::json::array();
        for (const auto& i : instruments) {
            response.push_back(instrumentToJson(i));
        }

        okJson(res, response);
    }

    void handleSearchInstruments(IRequest& req, IResponse& res)
    {
        const auto params = getParams(req);

        auto it = params.find("query");
        if (it == params.end() || it->second.empty()) {
            badRequest(res, "Query parameter is required");
            return;
        }

        const auto instruments = marketService_->searchInstruments(it->second);

        nlohmann::json response = nlohmann::json::array();
        for (const auto& i : instruments) {
            response.push_back(instrumentToJson(i));
        }

        okJson(res, response);
    }

    void handleGetInstrumentByFigi(const std::string& path, IResponse& res)
    {
        constexpr const char* Prefix = "/api/v1/instruments/";
        const std::string figi = path.substr(std::strlen(Prefix));

        if (figi.empty()) {
            badRequest(res, "FIGI is required");
            return;
        }

        auto instrument = marketService_->getInstrumentByFigi(figi);
        if (!instrument) {
            notFound(res, "Instrument not found");
            return;
        }

        okJson(res, instrumentToJson(*instrument));
    }

    /* ===================== responses ===================== */

    static void okJson(IResponse& res, const nlohmann::json& body)
    {
        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(body.dump());
    }

    static void badRequest(IResponse& res, std::string_view message)
    {
        res.setStatus(400);
        res.setHeader("Content-Type", "application/json");
        res.setBody(nlohmann::json{{"error", message}}.dump());
    }

    static void notFound(IResponse& res, std::string_view message = "Not found")
    {
        res.setStatus(404);
        res.setHeader("Content-Type", "application/json");
        res.setBody(nlohmann::json{{"error", message}}.dump());
    }

    static void methodNotAllowed(IResponse& res)
    {
        res.setStatus(405);
        res.setHeader("Allow", "GET");
        res.setHeader("Content-Type", "application/json");
        res.setBody(R"({"error":"Method not allowed"})");
    }

    /* ===================== json ===================== */

    static nlohmann::json quoteToJson(const domain::Quote& quote)
    {
        return {
            {"figi", quote.figi},
            {"ticker", quote.ticker},
            {"last_price", quote.lastPrice.toDouble()},
            {"bid_price", quote.bidPrice.toDouble()},
            {"ask_price", quote.askPrice.toDouble()},
            {"currency", quote.lastPrice.currency},
            {"updated_at", quote.updatedAt.toString()}
        };
    }

    static nlohmann::json instrumentToJson(const domain::Instrument& instr)
    {
        return {
            {"figi", instr.figi},
            {"ticker", instr.ticker},
            {"name", instr.name},
            {"currency", instr.currency},
            {"lot", instr.lot},
            {"trading_available", instr.tradingAvailable}
        };
    }
};

} // namespace trading::adapters::primary
