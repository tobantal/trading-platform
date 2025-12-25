#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IOrderService.hpp"
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary {

/**
 * @brief HTTP Handler для управления ордерами
 * 
 * Endpoints:
 * - POST /api/v1/orders
 * - GET /api/v1/orders
 * - GET /api/v1/orders/{id}
 * - DELETE /api/v1/orders/{id}
 */
class OrderHandler : public IHttpHandler
{
public:
    OrderHandler(
        std::shared_ptr<ports::input::IOrderService> orderService,
        std::shared_ptr<ports::input::IAuthService> authService,
        std::shared_ptr<ports::input::IAccountService> accountService
    ) : orderService_(std::move(orderService))
      , authService_(std::move(authService))
      , accountService_(std::move(accountService))
    {
        std::cout << "[OrderHandler] Created" << std::endl;
    }

    void handle(IRequest& req, IResponse& res) override
    {
        // Проверяем авторизацию
        auto userId = extractUserId(req);
        if (!userId) {
            res.setStatus(401);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Unauthorized"})");
            return;
        }

        // Получаем активный счёт
        auto account = accountService_->getActiveAccount(*userId);
        if (!account) {
            res.setStatus(400);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "No active account found"})");
            return;
        }

        std::string method = req.getMethod();
        std::string path = req.getPath();
        
        if (method == "POST" && path == "/api/v1/orders") {
            handleCreateOrder(req, res, account->id);
        } else if (method == "GET" && path == "/api/v1/orders") {
            handleGetOrders(req, res, account->id);
        } else if (method == "GET" && path.find("/api/v1/orders/") == 0) {
            handleGetOrder(req, res, account->id);
        } else if (method == "DELETE" && path.find("/api/v1/orders/") == 0) {
            handleCancelOrder(req, res, account->id);
        } else {
            res.setStatus(404);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Not found"})");
        }
    }

private:
    std::shared_ptr<ports::input::IOrderService> orderService_;
    std::shared_ptr<ports::input::IAuthService> authService_;
    std::shared_ptr<ports::input::IAccountService> accountService_;

    void handleCreateOrder(IRequest& req, IResponse& res, const std::string& accountId)
    {
        try {
            auto body = nlohmann::json::parse(req.getBody());

            domain::OrderRequest orderReq;
            orderReq.accountId = accountId;
            orderReq.figi = body.value("figi", "");
            orderReq.quantity = body.value("quantity", 0);
            
            std::string direction = body.value("direction", "BUY");
            orderReq.direction = (direction == "SELL") 
                ? domain::OrderDirection::SELL 
                : domain::OrderDirection::BUY;
            
            std::string type = body.value("type", "MARKET");
            orderReq.type = (type == "LIMIT") 
                ? domain::OrderType::LIMIT 
                : domain::OrderType::MARKET;

            if (orderReq.type == domain::OrderType::LIMIT) {
                orderReq.price = domain::Money::fromDouble(
                    body.value("price", 0.0),
                    body.value("currency", "RUB")
                );
            }

            // Валидация
            if (orderReq.figi.empty()) {
                res.setStatus(400);
                res.setHeader("Content-Type", "application/json");
                res.setBody(R"({"error": "FIGI is required"})");
                return;
            }
            if (orderReq.quantity <= 0) {
                res.setStatus(400);
                res.setHeader("Content-Type", "application/json");
                res.setBody(R"({"error": "Quantity must be positive"})");
                return;
            }

            auto result = orderService_->placeOrder(orderReq);

            nlohmann::json response;
            response["order_id"] = result.orderId;
            response["status"] = domain::toString(result.status);
            response["message"] = result.message;
            if (result.executedPrice.toDouble() > 0) {
                response["executed_price"] = result.executedPrice.toDouble();
            }
            response["timestamp"] = result.timestamp.toString();

            int httpStatus = (result.status == domain::OrderStatus::REJECTED) ? 400 : 201;
            res.setStatus(httpStatus);
            res.setHeader("Content-Type", "application/json");
            res.setBody(response.dump());

        } catch (const nlohmann::json::exception& e) {
            res.setStatus(400);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Invalid JSON"})");
        }
    }

    void handleGetOrders(IRequest& req, IResponse& res, const std::string& accountId)
    {
        auto orders = orderService_->getAllOrders(accountId);

        nlohmann::json response = nlohmann::json::array();
        for (const auto& order : orders) {
            response.push_back(orderToJson(order));
        }

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
    }

    void handleGetOrder(IRequest& req, IResponse& res, const std::string& accountId)
    {
        std::string path = req.getPath();
        std::string orderId = path.substr(std::string("/api/v1/orders/").length());
        
        auto order = orderService_->getOrderById(orderId);
        if (!order || order->accountId != accountId) {
            res.setStatus(404);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Order not found"})");
            return;
        }

        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(orderToJson(*order).dump());
    }

    void handleCancelOrder(IRequest& req, IResponse& res, const std::string& accountId)
    {
        std::string path = req.getPath();
        std::string orderId = path.substr(std::string("/api/v1/orders/").length());
        
        bool cancelled = orderService_->cancelOrder(accountId, orderId);
        
        if (cancelled) {
            nlohmann::json response;
            response["message"] = "Order cancelled";
            response["order_id"] = orderId;
            res.setStatus(200);
            res.setHeader("Content-Type", "application/json");
            res.setBody(response.dump());
        } else {
            res.setStatus(400);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Cannot cancel order"})");
        }
    }

    nlohmann::json orderToJson(const domain::Order& order)
    {
        nlohmann::json j;
        j["id"] = order.id;
        j["account_id"] = order.accountId;
        j["figi"] = order.figi;
        j["direction"] = domain::toString(order.direction);
        j["type"] = domain::toString(order.type);
        j["quantity"] = order.quantity;
        j["price"] = order.price.toDouble();
        j["currency"] = order.price.currency;
        j["status"] = domain::toString(order.status);
        j["created_at"] = order.createdAt.toString();
        j["updated_at"] = order.updatedAt.toString();
        return j;
    }

    std::optional<std::string> extractUserId(IRequest& req)
    {
        auto headers = req.getHeaders();
        auto it = headers.find("Authorization");
        if (it == headers.end()) {
            return std::nullopt;
        }

        std::string auth = it->second;
        if (auth.find("Bearer ") != 0) {
            return std::nullopt;
        }

        std::string token = auth.substr(7);
        return authService_->getUserIdFromToken(token);
    }
};

} // namespace trading::adapters::primary
