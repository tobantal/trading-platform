#pragma once

#include <IHttpHandler.hpp>
#include "ports/output/IAuthClient.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary
{

    /**
     * @brief Middleware для извлечения accountId и добавления в attributes.
     *
     * Требует Access Token (содержит accountId).
     */
    class AccountIdExtractorMiddleware : public IHttpHandler
    {
    public:
        AccountIdExtractorMiddleware(
            std::shared_ptr<ports::output::IAuthClient> authClient) : authClient_(std::move(authClient))
        {
            std::cout << "[AccountIdExctractorMiddleware] Created" << std::endl;
        }

        void handle(IRequest &req, IResponse &res) override
        {
            std::string token = req.getBearerToken().value_or("");
            if (token.empty())
            {
                sendError(res, 401, "Access token required. Use POST /api/v1/auth/select-account to get one.");
                return;
            }

            auto accountId = authClient_->getAccountIdFromToken(token).value_or("");
            if (accountId.empty())
            {
                sendError(res, 401, "Token not valid. Use POST /api/v1/auth/select-account to get one.");
                return;
            }

            req.setAttribute("accountId", accountId);
            res.setStatus(0); // для middleware
        }

    private:
        std::shared_ptr<ports::output::IAuthClient> authClient_;

        void sendError(IResponse &res, int status, const std::string &message)
        {
            nlohmann::json error;
            error["error"] = message;
            res.setResult(status, "application/json", error.dump());
        }
    };

} // namespace trading::adapters::primary
