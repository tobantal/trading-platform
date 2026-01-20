#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IMarketService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary
{

    /**
     * @brief GET /api/v1/instruments/{figi} — инструмент по FIGI
     *
     * Роутер регистрирует с паттерном "/api/v1/instruments/*"
     */
    class GetInstrumentByFigiHandler : public IHttpHandler
    {
    public:
        explicit GetInstrumentByFigiHandler(std::shared_ptr<ports::input::IMarketService> marketService)
            : marketService_(std::move(marketService))
        {
            std::cout << "[GetInstrumentByFigiHandler] Created" << std::endl;
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
                auto figi = req.getPathParam(0).value_or("");

                if (figi.empty())
                {
                    sendError(res, 400, "FIGI is required");
                    return;
                }

                auto instrument = marketService_->getInstrumentByFigi(figi);

                if (!instrument)
                {
                    sendError(res, 404, "Instrument not found");
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
            catch (const std::exception &e)
            {
                std::cerr << "[GetInstrumentByFigiHandler] Error: " << e.what() << std::endl;
                sendError(res, 500, "Internal server error");
            }
        }

    private:
        std::shared_ptr<ports::input::IMarketService> marketService_;

        void sendError(IResponse &res, int status, const std::string &message)
        {
            nlohmann::json error;
            error["error"] = message;
            res.setResult(status, "application/json", error.dump());
        }
    };

} // namespace trading::adapters::primary
