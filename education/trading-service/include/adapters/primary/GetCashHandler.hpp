#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IPortfolioService.hpp"
#include "ports/output/IAuthClient.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary
{

    /**
     * @brief GET /api/v1/portfolio/cash — доступные средства
     *
     * Требует Access Token (содержит accountId).
     */
    class GetCashHandler : public IHttpHandler
    {
    public:
        GetCashHandler(
            std::shared_ptr<ports::input::IPortfolioService> portfolioService,
            std::shared_ptr<ports::output::IAuthClient> authClient) : portfolioService_(std::move(portfolioService)), authClient_(std::move(authClient))
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

            auto accountId = extractAccountId(req);
            if (!accountId)
            {
                sendError(res, 401, "Access token required. Use POST /api/v1/auth/select-account to get one.");
                return;
            }

            try
            {
                auto cash = portfolioService_->getAvailableCash(*accountId);

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
        std::shared_ptr<ports::output::IAuthClient> authClient_;

        std::optional<std::string> extractAccountId(IRequest &req)
        {
            std::string token = req.getBearerToken().value_or("");
            return authClient_->getAccountIdFromToken(token);
        }

        void sendError(IResponse &res, int status, const std::string &message)
        {
            nlohmann::json error;
            error["error"] = message;
            res.setResult(status, "application/json", error.dump());
        }
    };

} // namespace trading::adapters::primary
