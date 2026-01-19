#pragma once

#include <IHttpHandler.hpp>
#include "ports/output/IBrokerGateway.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace broker::adapters::primary
{

    /**
     * @brief GET /api/v1/orders?account_id=xxx — список ордеров
     */
    class GetOrdersHandler : public IHttpHandler
    {
    public:
        explicit GetOrdersHandler(std::shared_ptr<broker::ports::output::IBrokerGateway> broker)
            : broker_(std::move(broker))
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

            try
            {
                std::string accountId = req.getQueryParam("account_id").value_or("");

                if (accountId.empty())
                {
                    sendError(res, 400, "Parameter 'account_id' is required");
                    return;
                }

                auto orders = broker_->getOrders(accountId);

                nlohmann::json response = nlohmann::json::array();
                for (const auto &order : orders)
                {
                    response.push_back(orderToJson(order));
                }

                res.setResult(200, "application/json", response.dump());
            }
            catch (const std::runtime_error &e)
            {
                sendError(res, 404, e.what());
            }
            catch (const std::exception &e)
            {
                std::cerr << "[GetOrdersHandler] Error: " << e.what() << std::endl;
                sendError(res, 500, std::string("Internal server error: ") + e.what());
            }
        }

    private:
        std::shared_ptr<broker::ports::output ::IBrokerGateway> broker_;

        nlohmann::json orderToJson(const broker::domain::Order &order)
        {
            nlohmann::json j;
            j["order_id"] = order.id;
            j["account_id"] = order.accountId;
            j["figi"] = order.figi;
            j["direction"] = broker::domain::toString(order.direction);
            j["order_type"] = broker::domain::toString(order.type);
            j["quantity"] = order.quantity;
            j["price"] = {
                {"units", order.price.units},
                {"nano", order.price.nano},
                {"currency", order.price.currency}};
            j["status"] = broker::domain::toString(order.status);
            j["executed_price"] = order.executedPrice.toDouble();
            j["executed_quantity"] = order.executedQuantity;
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
