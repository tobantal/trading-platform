#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IOrderService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary
{

    class GetOrdersHandler : public IHttpHandler
    {
    public:
        explicit GetOrdersHandler(
            std::shared_ptr<ports::input::IOrderService> orderService)
            : orderService_(std::move(orderService))
        {
            std::cout << "[GetOrdersHandler] Created" << std::endl;
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
                std::cout << "[GetOrdersHandler] Error: accountId must not be null on this step." << std::endl;
                return;
            }

            try
            {
                auto orders = orderService_->getAllOrders(accountId);

                nlohmann::json response;
                response["orders"] = nlohmann::json::array();
                for (const auto &order : orders)
                {
                    response["orders"].push_back(orderToJson(order));
                }

                res.setResult(200, "application/json", response.dump());
            }
            catch (const std::exception &e)
            {
                std::cerr << "[GetOrdersHandler] Error: " << e.what() << std::endl;
                sendError(res, 500, "Internal server error");
            }
        }

    private:
        std::shared_ptr<ports::input::IOrderService> orderService_;

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
