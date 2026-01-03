#pragma once

#include <IHttpHandler.hpp>
#include <IRequest.hpp>
#include <IResponse.hpp>
#include "ports/output/IBrokerGateway.hpp"
#include "domain/OrderRequest.hpp"
#include "domain/Money.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace broker::adapters::primary {

/**
 * @brief HTTP handler for orders endpoints
 * 
 * Endpoints:
 * - GET    /api/v1/orders?account_id=...           - список ордеров
 * - GET    /api/v1/orders/{id}?account_id=...      - ордер по ID
 * - POST   /api/v1/orders                          - создать ордер
 * - DELETE /api/v1/orders/{id}?account_id=...      - отменить ордер
 */
class OrdersHandler : public IHttpHandler {
public:
    explicit OrdersHandler(std::shared_ptr<ports::output::IBrokerGateway> broker)
        : broker_(std::move(broker))
    {
        std::cout << "[OrdersHandler] Created" << std::endl;
    }

    void handle(IRequest& req, IResponse& res) override {
        std::string method = req.getMethod();
        std::string path = req.getPath();
        std::string basePath = "/api/v1/orders";
        
        try {
            if (method == "GET") {
                handleGet(req, res, path, basePath);
            } else if (method == "POST" && path == basePath) {
                handlePlaceOrder(req, res);
            } else if (method == "DELETE" && path.find(basePath + "/") == 0) {
                handleCancelOrder(req, res, path, basePath);
            } else {
                sendError(res, 405, "Method not allowed");
            }
        } catch (const std::runtime_error& e) {
            // Account not found → 404
            sendError(res, 404, e.what());
        } catch (const std::exception& e) {
            std::cerr << "[OrdersHandler] Error: " << e.what() << std::endl;
            sendError(res, 500, std::string("Internal server error: ") + e.what());
        }
    }

private:
    std::shared_ptr<ports::output::IBrokerGateway> broker_;

    void handleGet(IRequest& req, IResponse& res, const std::string& path, const std::string& basePath) {
        // Извлекаем account_id из query параметров
        auto params = req.getParams();
        auto it = params.find("account_id");
        
        if (it == params.end() || it->second.empty()) {
            sendError(res, 400, "Parameter 'account_id' is required");
            return;
        }
        
        std::string accountId = it->second;
        
        if (path == basePath) {
            // GET /api/v1/orders - список всех ордеров
            handleGetOrders(res, accountId);
        } else if (path.find(basePath + "/") == 0) {
            // GET /api/v1/orders/{id} - конкретный ордер
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
     * @brief GET /api/v1/orders - список ордеров
     */
    void handleGetOrders(IResponse& res, const std::string& accountId) {
        auto orders = broker_->getOrders(accountId);
        
        // Postman ожидает массив напрямую, не объект с полем orders
        nlohmann::json response = nlohmann::json::array();
        
        for (const auto& order : orders) {
            response.push_back(orderToJson(order));
        }
        
        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

    /**
     * @brief GET /api/v1/orders/{id} - ордер по ID
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

    /**
     * @brief POST /api/v1/orders - создать ордер
     * 
     * Body:
     * {
     *   "account_id": "acc-001-sandbox",
     *   "figi": "BBG004730N88",
     *   "quantity": 10,
     *   "direction": "BUY",
     *   "order_type": "MARKET",
     *   "price": {"units": 260, "nano": 0, "currency": "RUB"}  // для LIMIT
     * }
     */
    void handlePlaceOrder(IRequest& req, IResponse& res) {
        std::string body = req.getBody();
        
        if (body.empty()) {
            sendError(res, 400, "Request body is required");
            return;
        }
        
        nlohmann::json j;
        try {
            j = nlohmann::json::parse(body);
        } catch (const std::exception& e) {
            sendError(res, 400, "Invalid JSON");
            return;
        }
        
        // Validate required fields
        if (!j.contains("account_id") || !j.contains("figi") || 
            !j.contains("quantity") || !j.contains("direction") || 
            !j.contains("order_type")) {
            sendError(res, 400, "Missing required fields: account_id, figi, quantity, direction, order_type");
            return;
        }
        
        std::string accountId = j["account_id"];
        std::string figi = j["figi"];
        int64_t quantity = j["quantity"];
        std::string direction = j["direction"];
        std::string orderType = j["order_type"];
        
        // Parse direction
        domain::OrderDirection dir;
        if (direction == "BUY") {
            dir = domain::OrderDirection::BUY;
        } else if (direction == "SELL") {
            dir = domain::OrderDirection::SELL;
        } else {
            sendError(res, 400, "Invalid direction. Must be BUY or SELL");
            return;
        }
        
        // Parse order type
        domain::OrderType type;
        if (orderType == "MARKET") {
            type = domain::OrderType::MARKET;
        } else if (orderType == "LIMIT") {
            type = domain::OrderType::LIMIT;
        } else {
            sendError(res, 400, "Invalid order_type. Must be MARKET or LIMIT");
            return;
        }
        
        // Parse price (required for LIMIT orders)
        domain::Money price = domain::Money::fromDouble(0, "RUB");
        if (type == domain::OrderType::LIMIT) {
            if (!j.contains("price")) {
                sendError(res, 400, "Price is required for LIMIT orders");
                return;
            }
            auto& priceJson = j["price"];
            int64_t units = priceJson.value("units", 0);
            int64_t nano = priceJson.value("nano", 0);
            std::string currency = priceJson.value("currency", "RUB");
            price = domain::Money(units, nano, currency);
        }
        
        // Create order request
        domain::OrderRequest request;
        request.figi = figi;
        request.quantity = quantity;
        request.direction = dir;
        request.type = type;
        request.price = price;
        
        // Place order
        auto result = broker_->placeOrder(accountId, request);
        
        // Build response
        nlohmann::json response;
        response["order_id"] = result.orderId;
        response["status"] = statusToString(result.status);
        response["executed_lots"] = result.executedLots;
        if (result.executedLots > 0) {
            response["executed_price"] = {
                {"units", static_cast<int64_t>(result.executedPrice.toDouble())},
                {"nano", 0},
                {"currency", result.executedPrice.currency}
            };
        }
        if (!result.message.empty()) {
            response["message"] = result.message;
        }
        
        res.setStatus(201);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

    /**
     * @brief DELETE /api/v1/orders/{id} - отменить ордер
     */
    void handleCancelOrder(IRequest& req, IResponse& res, const std::string& path, const std::string& basePath) {
        // Извлекаем account_id
        auto params = req.getParams();
        auto it = params.find("account_id");
        
        if (it == params.end() || it->second.empty()) {
            sendError(res, 400, "Parameter 'account_id' is required");
            return;
        }
        
        std::string accountId = it->second;
        std::string orderId = extractOrderId(path, basePath);
        
        if (orderId.empty()) {
            sendError(res, 400, "Order ID is required");
            return;
        }
        
        bool cancelled = broker_->cancelOrder(accountId, orderId);
        
        if (!cancelled) {
            sendError(res, 404, "Order not found or cannot be cancelled");
            return;
        }
        
        nlohmann::json response;
        response["order_id"] = orderId;
        response["status"] = "CANCELLED";
        
        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

    std::string extractOrderId(const std::string& path, const std::string& basePath) {
        std::string orderId = path.substr(basePath.length() + 1);
        
        // Убираем query string если есть
        size_t qPos = orderId.find('?');
        if (qPos != std::string::npos) {
            orderId = orderId.substr(0, qPos);
        }
        
        return orderId;
    }

    nlohmann::json orderToJson(const domain::Order& order) {
        nlohmann::json j;
        j["order_id"] = order.id;  // domain::Order использует id, не orderId
        j["account_id"] = order.accountId;
        j["figi"] = order.figi;
        j["direction"] = directionToString(order.direction);
        j["order_type"] = orderTypeToString(order.type);
        j["quantity"] = order.quantity;
        // domain::Order не имеет filledQuantity - это есть только в BrokerOrder
        j["price"] = {
            {"units", static_cast<int64_t>(order.price.toDouble())},
            {"nano", 0},
            {"currency", order.price.currency}
        };
        j["status"] = statusToString(order.status);
        return j;
    }

    std::string directionToString(domain::OrderDirection dir) {
        return (dir == domain::OrderDirection::BUY) ? "BUY" : "SELL";
    }

    std::string orderTypeToString(domain::OrderType type) {
        return (type == domain::OrderType::MARKET) ? "MARKET" : "LIMIT";
    }

    std::string statusToString(domain::OrderStatus status) {
        switch (status) {
            case domain::OrderStatus::PENDING: return "NEW";
            case domain::OrderStatus::FILLED: return "FILLED";
            case domain::OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
            case domain::OrderStatus::CANCELLED: return "CANCELLED";
            case domain::OrderStatus::REJECTED: return "REJECTED";
            default: return "UNKNOWN";
        }
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
