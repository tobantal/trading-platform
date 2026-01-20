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
     * @brief GET /api/v1/orders/{id} — ордер по ID
     *
     * Роутер регистрирует с паттерном "/api/v1/orders/*"
     * Требует Access Token (содержит accountId).
     */
    class GetOrderHandler : public IHttpHandler
    {
    public:
        GetOrderHandler(
            std::shared_ptr<ports::input::IOrderService> orderService,
            std::shared_ptr<ports::output::IAuthClient> authClient) : orderService_(std::move(orderService)), authClient_(std::move(authClient))
        {
            std::cout << "[GetOrderHandler] Created" << std::endl;
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
                std::string orderId = req.getPathParam(0).value_or("");

                if (orderId.empty())
                {
                    sendError(res, 400, "Order ID is required");
                    return;
                }

                auto order = orderService_->getOrderById(*accountId, orderId);

                if (!order)
                {
                    sendError(res, 404, "Order not found");
                    return;
                }

                res.setResult(200, "application/json", orderToJson(*order).dump());
            }
            catch (const std::exception &e)
            {
                std::cerr << "[GetOrderHandler] Error: " << e.what() << std::endl;
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

        nlohmann::json orderToJson(const domain::Order &order)
        {
            nlohmann::json j;
            j["order_id"] = order.id;
            j["account_id"] = order.accountId;
            j["figi"] = order.figi;
            j["direction"] = domain::toString(order.direction);
            j["type"] = domain::toString(order.type);
            j["quantity"] = order.quantity;
            j["price"] = order.price.toDouble();
            j["executed_price"] = order.executedPrice.toDouble();
            j["executed_quantity"] = order.executedQuantity;
            j["currency"] = order.price.currency;
            j["status"] = domain::toString(order.status);
            j["created_at"] = order.createdAt.toString();
            j["updated_at"] = order.updatedAt.toString();
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
