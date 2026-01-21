#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IPortfolioService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary
{

    class GetCashHandler : public IHttpHandler
    {
    public:
        explicit GetCashHandler(
            std::shared_ptr<ports::input::IPortfolioService> portfolioService)
            : portfolioService_(std::move(portfolioService))
        {
            std::cout << "[GetCashHandler] Created" << std::endl;
        }

        void handle(IRequest &req, IResponse &res) override
        {
            if (req.getMethod() != "GET")
            {
                sendError(res, 405, "Method not allowed");
                return;
            }

            auto accountId = req.getAttribute("accountId").value_or("");
            if (accountId.empty())
            {
                sendError(res, 500, "Internal server error");
                std::cout << "[GetCashHandler] Error: accountId must not be null on this step." << std::endl;
                return;
            }

            try
            {
                auto cash = portfolioService_->getAvailableCash(accountId);

                nlohmann::json response;
                response["amount"] = cash.toDouble();
                response["currency"] = cash.currency;

                res.setResult(200, "application/json", response.dump());
            }
            catch (const std::exception &e)
            {
                std::cerr << "[GetCashHandler] Error: " << e.what() << std::endl;
                sendError(res, 500, "Internal server error");
            }
        }

    private:
        std::shared_ptr<ports::input::IPortfolioService> portfolioService_;

        void sendError(IResponse &res, int status, const std::string &message)
        {
            nlohmann::json error;
            error["error"] = message;
            res.setResult(status, "application/json", error.dump());
        }
    };

} // namespace trading::adapters::primary
