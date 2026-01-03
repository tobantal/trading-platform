#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <iostream>

namespace trading::application {

/**
 * @brief Обработчик событий ордеров от broker-service
 * 
 * Обрабатывает события:
 * - order.created - ордер принят
 * - order.rejected - ордер отклонён
 * - order.filled - ордер исполнен
 * - order.cancelled - ордер отменён
 */
class OrderEventHandler {
public:
    OrderEventHandler() {
        std::cout << "[OrderEventHandler] Created" << std::endl;
    }

    void handle(const std::string& routingKey, const std::string& message) {
        std::cout << "[OrderEventHandler] Received " << routingKey << std::endl;

        try {
            auto json = nlohmann::json::parse(message);

            if (routingKey == "order.created") {
                handleOrderCreated(json);
            } else if (routingKey == "order.rejected") {
                handleOrderRejected(json);
            } else if (routingKey == "order.filled") {
                handleOrderFilled(json);
            } else if (routingKey == "order.cancelled") {
                handleOrderCancelled(json);
            } else {
                std::cout << "[OrderEventHandler] Unknown event: " << routingKey << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[OrderEventHandler] Error parsing message: " << e.what() << std::endl;
        }
    }

private:
    void handleOrderCreated(const nlohmann::json& json) {
        std::string orderId = json.value("order_id", "");
        std::string accountId = json.value("account_id", "");
        std::string status = json.value("status", "");
        
        std::cout << "[OrderEventHandler] Order created: " << orderId 
                  << " account=" << accountId 
                  << " status=" << status << std::endl;
        
        // TODO: Можно уведомить клиента через WebSocket
    }

    void handleOrderRejected(const nlohmann::json& json) {
        std::string orderId = json.value("order_id", "");
        std::string reason = json.value("reason", "");
        
        std::cout << "[OrderEventHandler] Order rejected: " << orderId 
                  << " reason=" << reason << std::endl;
        
        // TODO: Можно уведомить клиента через WebSocket
    }

    void handleOrderFilled(const nlohmann::json& json) {
        std::string orderId = json.value("order_id", "");
        double executedPrice = json.value("executed_price", 0.0);
        int executedQuantity = json.value("executed_quantity", 0);
        
        std::cout << "[OrderEventHandler] Order filled: " << orderId 
                  << " price=" << executedPrice 
                  << " qty=" << executedQuantity << std::endl;
        
        // TODO: Можно уведомить клиента через WebSocket
    }

    void handleOrderCancelled(const nlohmann::json& json) {
        std::string orderId = json.value("order_id", "");
        
        std::cout << "[OrderEventHandler] Order cancelled: " << orderId << std::endl;
        
        // TODO: Можно уведомить клиента через WebSocket
    }
};

} // namespace trading::application
