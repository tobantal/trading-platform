#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IMarketService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <sstream>
#include <iostream>

namespace trading::adapters::primary
{

    /**
     * @brief GET /api/v1/quotes?figis=... — котировки по списку FIGI
     */
    class GetQuotesHandler : public IHttpHandler
    {
    public:
        explicit GetQuotesHandler(std::shared_ptr<ports::input::IMarketService> marketService)
            : marketService_(std::move(marketService))
        {
            std::cout << "[GetQuotesHandler] Created" << std::endl;
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
                auto figis = req.getQueryParam("figis").value_or("");

                if (figis.empty())
                {
                    sendError(res, 400, "Parameter 'figis' is required");
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
            catch (const std::exception &e)
            {
                std::cerr << "[GetQuotesHandler] Error: " << e.what() << std::endl;
                sendError(res, 500, "Internal server error");
            }
        }

    private:
        std::shared_ptr<ports::input::IMarketService> marketService_;

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

        void sendError(IResponse &res, int status, const std::string &message)
        {
            nlohmann::json error;
            error["error"] = message;
            res.setResult(status, "application/json", error.dump());
        }
    };

} // namespace trading::adapters::primary
