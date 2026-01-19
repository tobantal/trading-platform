#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IMarketService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <sstream>

namespace trading::adapters::primary
{

    /**
     * @brief HTTP Handler для рыночных данных
     *
     * Endpoints (без авторизации):
     * - GET /api/v1/instruments
     * - GET /api/v1/instruments/{figi}
     * - GET /api/v1/instruments/search?query=...
     * - GET /api/v1/quotes?figis=...
     */
    class MarketHandler final : public IHttpHandler
    {
    public:
        explicit MarketHandler(std::shared_ptr<ports::input::IMarketService> marketService)
            : marketService_(std::move(marketService))
        {
        }

        void handle(IRequest &req, IResponse &res) override
        {
            if (req.getMethod() != "GET")
            {
                methodNotAllowed(res);
                return;
            }

            const std::string path = req.getPath();

            if (path == "/api/v1/quotes")
            {
                handleGetQuotes(req, res);
                return;
            }

            if (path == "/api/v1/instruments")
            {
                handleGetInstruments(req, res);
                return;
            }

            if (path == "/api/v1/instruments/search")
            {
                handleSearchInstruments(req, res);
                return;
            }

            if (req.getPathPattern() == "/api/v1/instruments/*")
            {
                handleGetInstrumentByFigi(req, res);
                return;
            }

            notFound(res);
        }

    private:
        std::shared_ptr<ports::input::IMarketService> marketService_;

        /**
         * @brief Парсит список FIGI из строки через запятую
         */
        static std::vector<std::string> parseFigis(const std::string &figisStr)
        {
            std::vector<std::string> figis;
            std::stringstream ss(figisStr);
            std::string figi;

            while (std::getline(ss, figi, ','))
            {
                if (!figi.empty())
                {
                    figis.push_back(figi);
                }
            }

            return figis;
        }

        /**
         * @brief GET /api/v1/quotes?figis=...
         */
        void handleGetQuotes(IRequest &req, IResponse &res)
        {
            auto figis = req.getQueryParam("figis").value_or("");
            if (figis.empty())
            {
                res.setResult(400, "application/json", R"({"error": "Parameter 'figis' is required"})");
                return;
            }
            auto parsedFigis = parseFigis(figis);

            auto quotes = marketService_->getQuotes(parsedFigis);

            nlohmann::json response;
            response["quotes"] = nlohmann::json::array();

            for (const auto &quote : quotes)
            {
                nlohmann::json q;
                q["figi"] = quote.figi;
                q["ticker"] = quote.ticker;
                q["last_price"] = quote.lastPrice.toDouble();
                q["bid_price"] = quote.bidPrice.toDouble();
                q["ask_price"] = quote.askPrice.toDouble();
                q["currency"] = quote.lastPrice.currency;
                response["quotes"].push_back(q);
            }

            res.setResult(200, "application/json", response.dump());
        }

        /**
         * @brief GET /api/v1/instruments
         */
        void handleGetInstruments(IRequest &req, IResponse &res)
        {
            auto instruments = marketService_->getAllInstruments();

            nlohmann::json response;
            response["instruments"] = nlohmann::json::array();

            for (const auto &instr : instruments)
            {
                nlohmann::json i;
                i["figi"] = instr.figi;
                i["ticker"] = instr.ticker;
                i["name"] = instr.name;
                i["currency"] = instr.currency;
                i["lot"] = instr.lot;
                response["instruments"].push_back(i);
            }

            res.setResult(200, "application/json", response.dump());
        }

        /**
         * @brief GET /api/v1/instruments/search?query=...
         */
        void handleSearchInstruments(IRequest &req, IResponse &res)
        {
            auto query = req.getQueryParam("query").value_or("");
            if (query.empty())
            {
                res.setResult(400, "application/json", R"({"error": "Parameter 'query' is required"})");
                return;
            }

            auto instruments = marketService_->searchInstruments(query);

            nlohmann::json response;
            response["instruments"] = nlohmann::json::array();

            for (const auto &instr : instruments)
            {
                nlohmann::json i;
                i["figi"] = instr.figi;
                i["ticker"] = instr.ticker;
                i["name"] = instr.name;
                i["currency"] = instr.currency;
                i["lot"] = instr.lot;
                response["instruments"].push_back(i);
            }

            res.setResult(200, "application/json", response.dump());
        }

        /**
         * @brief GET /api/v1/instruments/{figi}
         */
        void handleGetInstrumentByFigi(IRequest &req, IResponse &res)
        {
            auto figi = req.getPathParam(0);

            if (!figi || figi->empty())
            {
                res.setResult(400, "application/json", R"({"error": "FIGI is required"})");
                return;
            }

            auto instrument = marketService_->getInstrumentByFigi(*figi);

            if (!instrument)
            {
                res.setResult(404, "application/json", R"({"error": "Instrument not found"})");
                return;
            }

            nlohmann::json response;
            response["figi"] = instrument->figi;
            response["ticker"] = instrument->ticker;
            response["name"] = instrument->name;
            response["currency"] = instrument->currency;
            response["lot"] = instrument->lot;

            res.setResult(200, "application/json", response.dump());
        }

        void methodNotAllowed(IResponse &res)
        {
            res.setResult(405, "application/json", R"({"error": "Method not allowed"})");
        }

        void notFound(IResponse &res)
        {
            res.setResult(404, "application/json", R"({"error": "Not found"})");
        }
    };

} // namespace trading::adapters::primary
