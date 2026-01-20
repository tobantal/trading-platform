#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IOrderService.hpp"
#include "ports/output/IAuthClient.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary
{

    /**
     * @brief DELETE /api/v1/orders/{id} — отменить ордер
     *
     * Роутер регистрирует с паттерном "/api/v1/orders/*"
     * Требует Access Token (содержит accountId).
     */
    class CancelOrderHandler : public IHttpHandler
    {
    public:
        CancelOrderHandler(
            std::shared_ptr<ports::input::IOrderService> orderService,
            std::shared_ptr<ports::output::IAuthClient> authClient) : orderService_(std::move(orderService)), authClient_(std::move(authClient))
        {
            std::cout << "[CancelOrderHandler] Created" << std::endl;
        }

        void handle(IRequest &req, IResponse &res) override
        {
            if (req.getMethod() != "DELETE")
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
                std::string orderId = req.getPathParam(0).value_or("");

                if (orderId.empty())
                {
                    sendError(res, 400, "Order ID is required");
                    return;
                }

                bool cancelled = orderService_->cancelOrder(*accountId, orderId);

                if (cancelled)
                {
                    nlohmann::json response;
                    response["message"] = "Order cancelled";
                    response["order_id"] = orderId;
                    res.setResult(200, "application/json", response.dump());
                }
                else
                {
                    sendError(res, 400, "Cannot cancel order");
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "[CancelOrderHandler] Error: " << e.what() << std::endl;
                sendError(res, 500, "Internal server error");
            }
        }

    private:
        std::shared_ptr<ports::input::IOrderService> orderService_;
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
