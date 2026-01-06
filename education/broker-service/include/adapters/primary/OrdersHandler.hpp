// include/adapters/primary/OrdersHandler.hpp
#pragma once

#include <IHttpHandler.hpp>
#include "ports/output/IBrokerGateway.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <iostream>

namespace broker::adapters::primary {

/**
 * @brief HTTP Handler для ордеров (ТОЛЬКО GET!)
 * 
 * POST и DELETE операции теперь через RabbitMQ:
 * - order.create → OrderCommandHandler
 * - order.cancel → OrderCommandHandler
 * 
 * Этот handler обрабатывает только чтение:
 * - GET /api/v1/orders?account_id=xxx — список ордеров
 * - GET /api/v1/orders/{id}?account_id=xxx — ордер по ID
 */
class OrdersHandler : public IHttpHandler {
public:
    explicit OrdersHandler(std::shared_ptr<ports::output::IBrokerGateway> broker)
        : broker_(std::move(broker))
    {
        std::cout << "[OrdersHandler] Created (GET only, POST/DELETE via RabbitMQ)" << std::endl;
    }

    void handle(IRequest& req, IResponse& res) override {
        std::string method = req.getMethod();
        std::string path = req.getPath();
        std::string basePath = "/api/v1/orders";
        
        try {
            if (method == "GET") {
                handleGet(req, res, path, basePath);
            } else if (method == "POST") {
                // POST теперь через RabbitMQ order.create
                sendError(res, 405, "POST /orders is now event-driven. "
                                   "Use trading-service POST /api/v1/orders which publishes to RabbitMQ.");
            } else if (method == "DELETE") {
                // DELETE теперь через RabbitMQ order.cancel
                sendError(res, 405, "DELETE /orders is now event-driven. "
                                   "Use trading-service DELETE /api/v1/orders/{id} which publishes to RabbitMQ.");
            } else {
                sendError(res, 405, "Method not allowed");
            }
        } catch (const std::runtime_error& e) {
            sendError(res, 404, e.what());
        } catch (const std::exception& e) {
            std::cerr << "[OrdersHandler] Error: " << e.what() << std::endl;
            sendError(res, 500, std::string("Internal server error: ") + e.what());
        }
    }

private:
    std::shared_ptr<ports::output::IBrokerGateway> broker_;

    void handleGet(IRequest& req, IResponse& res, const std::string& path, const std::string& basePath) {
        auto params = req.getParams();
        auto it = params.find("account_id");
        
        if (it == params.end() || it->second.empty()) {
            sendError(res, 400, "Parameter 'account_id' is required");
            return;
        }
        
        std::string accountId = it->second;
        
        if (path == basePath) {
            handleGetOrders(res, accountId);
        } else if (path.find(basePath + "/") == 0) {
            std::string orderId = extractOrderId(path, basePath);
            
            if (orderId.empty()) {
                handleGetOrders(res, accountId);
            } else {
                handleGetOrder(res, accountId, orderId);
            }
        } else {
            sendError(res, 404, "Not found");
        }
    }

    /**
     * @brief GET /api/v1/orders — список ордеров
     */
    void handleGetOrders(IResponse& res, const std::string& accountId) {
        auto orders = broker_->getOrders(accountId);
        
        nlohmann::json response = nlohmann::json::array();
        
        for (const auto& order : orders) {
            response.push_back(orderToJson(order));
        }
        
        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

    /**
     * @brief GET /api/v1/orders/{id} — ордер по ID
     */
    void handleGetOrder(IResponse& res, const std::string& accountId, const std::string& orderId) {
        auto orderOpt = broker_->getOrderStatus(accountId, orderId);
        
        if (!orderOpt) {
            sendError(res, 404, "Order not found");
            return;
        }
        
        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(orderToJson(*orderOpt).dump());
    }

    std::string extractOrderId(const std::string& path, const std::string& basePath) {
        std::string orderId = path.substr(basePath.length() + 1);
        
        size_t qPos = orderId.find('?');
        if (qPos != std::string::npos) {
            orderId = orderId.substr(0, qPos);
        }
        
        return orderId;
    }

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
        
        res.setStatus(status);
        res.setHeader("Content-Type", "application/json");
        res.setBody(error.dump());
    }
};

} // namespace broker::adapters::primary
