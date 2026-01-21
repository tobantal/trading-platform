#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IOrderService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary
{

    class CancelOrderHandler : public IHttpHandler
    {
    public:
        explicit CancelOrderHandler(
            std::shared_ptr<ports::input::IOrderService> orderService)
            : orderService_(std::move(orderService))
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

            auto accountId = req.getAttribute("accountId").value_or("");
            if (accountId.empty())
            {
                sendError(res, 500, "Internal server error");
                std::cout << "[CancelOrderHandler] Error: accountId must not be null on this step." << std::endl;
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

                bool cancelled = orderService_->cancelOrder(accountId, orderId);

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

        void sendError(IResponse &res, int status, const std::string &message)
        {
            nlohmann::json error;
            error["error"] = message;
            res.setResult(status, "application/json", error.dump());
        }
    };

} // namespace trading::adapters::primary
