#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IMarketService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary
{

    /**
     * @brief GET /api/v1/instruments/search?query=... — поиск инструментов
     */
    class SearchInstrumentsHandler : public IHttpHandler
    {
    public:
        explicit SearchInstrumentsHandler(std::shared_ptr<ports::input::IMarketService> marketService)
            : marketService_(std::move(marketService))
        {
            std::cout << "[SearchInstrumentsHandler] Created" << std::endl;
        }

        void handle(IRequest &req, IResponse &res) override
        {
            if (req.getMethod() != "GET")
            {
                sendError(res, 405, "Method not allowed");
                return;
            }

            try
            {
                auto query = req.getQueryParam("query").value_or("");

                if (query.empty())
                {
                    sendError(res, 400, "Parameter 'query' is required");
                    return;
                }

                auto instruments = marketService_->searchInstruments(query);

                nlohmann::json response;
                response["instruments"] = nlohmann::json::array();

                for (const auto &instr : instruments)
                {
                    response["instruments"].push_back(instrumentToJson(instr));
                }

                res.setResult(200, "application/json", response.dump());
            }
            catch (const std::exception &e)
            {
                std::cerr << "[SearchInstrumentsHandler] Error: " << e.what() << std::endl;
                sendError(res, 500, "Internal server error");
            }
        }

    private:
        std::shared_ptr<ports::input::IMarketService> marketService_;

        nlohmann::json instrumentToJson(const domain::Instrument &instr)
        {
            nlohmann::json j;
            j["figi"] = instr.figi;
            j["ticker"] = instr.ticker;
            j["name"] = instr.name;
            j["currency"] = instr.currency;
            j["lot"] = instr.lot;
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
