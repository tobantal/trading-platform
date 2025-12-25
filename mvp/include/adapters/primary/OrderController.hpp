#pragma once

#include "ports/input/IOrderService.hpp"
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include <http_server/IRequestHandler.hpp>
#include <nlohmann/json.hpp>
#include <memory>

namespace trading::adapters::primary {

/**
 * @brief REST контроллер управления ордерами
 * 
 * Endpoints:
 * - POST /api/v1/orders - создать ордер
 * - GET /api/v1/orders - список ордеров
 * - GET /api/v1/orders/{id} - получить ордер
 * - DELETE /api/v1/orders/{id} - отменить ордер
 */
class OrderController : public microservice::IRequestHandler {
public:
    OrderController(
        std::shared_ptr<ports::input::IOrderService> orderService,
        std::shared_ptr<ports::input::IAuthService> authService,
        std::shared_ptr<ports::input::IAccountService> accountService
    ) : orderService_(std::move(orderService))
      , authService_(std::move(authService))
      , accountService_(std::move(accountService))
    {}

    microservice::Response handle(const microservice::Request& request) override {
        // Проверяем авторизацию
        auto userId = extractUserId(request);
        if (!userId) {
            return unauthorized();
        }

        // Получаем активный счёт пользователя
        auto account = accountService_->getActiveAccount(*userId);
        if (!account) {
            return badRequest("No active account found");
        }

        // POST /api/v1/orders
        if (request.method == "POST" && request.path == "/api/v1/orders") {
            return handleCreateOrder(request, account->id);
        }
        
        // GET /api/v1/orders
        if (request.method == "GET" && request.path == "/api/v1/orders") {
            return handleGetOrders(request, account->id);
        }
        
        // GET /api/v1/orders/{id}
        if (request.method == "GET" && request.path.find("/api/v1/orders/") == 0) {
            return handleGetOrder(request, account->id);
        }
        
        // DELETE /api/v1/orders/{id}
        if (request.method == "DELETE" && request.path.find("/api/v1/orders/") == 0) {
            return handleCancelOrder(request, account->id);
        }

        return notFound();
    }

private:
    std::shared_ptr<ports::input::IOrderService> orderService_;
    std::shared_ptr<ports::input::IAuthService> authService_;
    std::shared_ptr<ports::input::IAccountService> accountService_;

    microservice::Response handleCreateOrder(const microservice::Request& request, const std::string& accountId) {
        try {
            auto body = nlohmann::json::parse(request.body);

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
                return badRequest("FIGI is required");
            }
            if (orderReq.quantity <= 0) {
                return badRequest("Quantity must be positive");
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
            return jsonResponse(httpStatus, response);

        } catch (const nlohmann::json::exception& e) {
            return badRequest("Invalid JSON: " + std::string(e.what()));
        }
    }

    microservice::Response handleGetOrders(const microservice::Request& request, const std::string& accountId) {
        auto orders = orderService_->getAllOrders(accountId);

        nlohmann::json response = nlohmann::json::array();
        for (const auto& order : orders) {
            response.push_back(orderToJson(order));
        }

        return jsonResponse(200, response);
    }

    microservice::Response handleGetOrder(const microservice::Request& request, const std::string& accountId) {
        std::string orderId = request.path.substr(std::string("/api/v1/orders/").length());
        
        auto order = orderService_->getOrderById(orderId);
        if (!order || order->accountId != accountId) {
            return notFound("Order not found");
        }

        return jsonResponse(200, orderToJson(*order));
    }

    microservice::Response handleCancelOrder(const microservice::Request& request, const std::string& accountId) {
        std::string orderId = request.path.substr(std::string("/api/v1/orders/").length());
        
        bool cancelled = orderService_->cancelOrder(accountId, orderId);
        
        if (cancelled) {
            nlohmann::json response;
            response["message"] = "Order cancelled";
            response["order_id"] = orderId;
            return jsonResponse(200, response);
        } else {
            return badRequest("Cannot cancel order");
        }
    }

    nlohmann::json orderToJson(const domain::Order& order) {
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

    std::optional<std::string> extractUserId(const microservice::Request& request) {
        // Извлекаем токен из заголовка Authorization: Bearer <token>
        auto it = request.headers.find("Authorization");
        if (it == request.headers.end()) {
            return std::nullopt;
        }

        std::string auth = it->second;
        if (auth.find("Bearer ") != 0) {
            return std::nullopt;
        }

        std::string token = auth.substr(7);
        return authService_->getUserIdFromToken(token);
    }

    microservice::Response jsonResponse(int status, const nlohmann::json& body) {
        microservice::Response resp;
        resp.status = status;
        resp.headers["Content-Type"] = "application/json";
        resp.body = body.dump();
        return resp;
    }

    microservice::Response badRequest(const std::string& message) {
        nlohmann::json body;
        body["error"] = message;
        return jsonResponse(400, body);
    }

    microservice::Response unauthorized() {
        nlohmann::json body;
        body["error"] = "Unauthorized";
        return jsonResponse(401, body);
    }

    microservice::Response notFound(const std::string& message = "Not found") {
        nlohmann::json body;
        body["error"] = message;
        return jsonResponse(404, body);
    }
};

} // namespace trading::adapters::primary
