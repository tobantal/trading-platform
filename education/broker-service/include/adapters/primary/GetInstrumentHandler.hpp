#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IQuoteService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace broker::adapters::primary
{

    /**
     * @brief GET /api/v1/instruments/{figi} — инструмент по FIGI
     *
     * Роутер регистрирует с паттерном "/api/v1/instruments/*"
     */
    class GetInstrumentHandler : public IHttpHandler
    {
    public:
        explicit GetInstrumentHandler(std::shared_ptr<ports::input::IQuoteService> quoteService)
            : quoteService_(std::move(quoteService))
        {
            std::cout << "[GetInstrumentHandler] Created" << std::endl;
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
                std::string figi = req.getPathParam(0).value_or("");

                if (figi.empty())
                {
                    sendError(res, 400, "FIGI is required");
                    return;
                }

                auto instrument = quoteService_->getInstrument(figi);

                if (!instrument)
                {
                    sendError(res, 404, "Instrument not found");
                    return;
                }

                res.setResult(200, "application/json", instrumentToJson(*instrument).dump());
            }
            catch (const std::exception &e)
            {
                std::cerr << "[GetInstrumentHandler] Error: " << e.what() << std::endl;
                sendError(res, 500, "Internal server error");
            }
        }

    private:
        std::shared_ptr<ports::input::IQuoteService> quoteService_;

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

} // namespace broker::adapters::primary
