#pragma once

#include <IHttpHandler.hpp>
#include <RouteMatcher.hpp>
#include "ports/output/IBrokerGateway.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace broker::adapters::primary {

/**
 * @brief GET /api/v1/orders/{id}?account_id=xxx — ордер по ID
 * 
 * Роутер регистрирует с паттерном "/api/v1/orders/*"
 */
class GetOrderHandler : public IHttpHandler {
public:
    explicit GetOrderHandler(std::shared_ptr<ports::output::IBrokerGateway> broker)
        : broker_(std::move(broker))
    {
        std::cout << "[GetOrderHandler] Created" << std::endl;
    }

    void handle(IRequest& req, IResponse& res) override {
        if (req.getMethod() != "GET") {
            sendError(res, 405, "Method not allowed");
            return;
        }

        try {
            std::string accountId = req.getQueryParam("account_id").value_or("");
            
            if (accountId.empty()) {
                sendError(res, 400, "Parameter 'account_id' is required");
                return;
            }

            std::string orderId = req.getPathParam(0).value_or("");
            
            if (orderId.empty()) {
                sendError(res, 400, "Order ID is required");
                return;
            }

            auto orderOpt = broker_->getOrderStatus(accountId, orderId);
            
            if (!orderOpt) {
                sendError(res, 404, "Order not found");
                return;
            }
            
            res.setResult(200, "application/json", orderToJson(*orderOpt).dump());

        } catch (const std::runtime_error& e) {
            sendError(res, 404, e.what());
        } catch (const std::exception& e) {
            std::cerr << "[GetOrderHandler] Error: " << e.what() << std::endl;
            sendError(res, 500, std::string("Internal server error: ") + e.what());
        }
    }

private:
    std::shared_ptr<ports::output::IBrokerGateway> broker_;

    nlohmann::json orderToJson(const domain::Order& order) {
        nlohmann::json j;
        j["order_id"] = order.id;
        j["account_id"] = order.accountId;
        j["figi"] = order.figi;
        j["direction"] = domain::toString(order.direction);
        j["order_type"] = domain::toString(order.type);
        j["quantity"] = order.quantity;
        j["price"] = {
            {"units", order.price.units},
            {"nano", order.price.nano},
            {"currency", order.price.currency}
        };
        j["status"] = domain::toString(order.status);
        j["executed_price"] = order.executedPrice.toDouble();
        j["executed_quantity"] = order.executedQuantity;
        return j;
    }

    void sendError(IResponse& res, int status, const std::string& message) {
        nlohmann::json error;
        error["error"] = message;
        res.setResult(status, "application/json", error.dump());
    }
};

} // namespace broker::adapters::primary
