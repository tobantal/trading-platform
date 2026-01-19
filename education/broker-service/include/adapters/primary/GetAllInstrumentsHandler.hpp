#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IQuoteService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace broker::adapters::primary
{

    /**
     * @brief GET /api/v1/instruments — список всех инструментов
     */
    class GetAllInstrumentsHandler : public IHttpHandler
    {
    public:
        explicit GetAllInstrumentsHandler(std::shared_ptr<ports::input::IQuoteService> quoteService)
            : quoteService_(std::move(quoteService))
        {
            std::cout << "[GetAllInstrumentsHandler] Created" << std::endl;
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
                auto instruments = quoteService_->getInstruments();

                nlohmann::json response = nlohmann::json::array();
                for (const auto &instr : instruments)
                {
                    response.push_back(instrumentToJson(instr));
                }

                res.setResult(200, "application/json", response.dump());
            }
            catch (const std::exception &e)
            {
                std::cerr << "[GetAllInstrumentsHandler] Error: " << e.what() << std::endl;
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
