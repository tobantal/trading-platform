// broker-service/include/application/OrderCommandHandler.hpp
#pragma once

#include "ports/output/IEventConsumer.hpp"
#include "ports/output/IEventPublisher.hpp"
#include "ports/output/IBrokerGateway.hpp"
#include "domain/OrderRequest.hpp"
#include "domain/OrderResult.hpp"
#include "domain/enums/OrderDirection.hpp"
#include "domain/enums/OrderType.hpp"
#include "domain/Money.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>
#include <chrono>

namespace broker::application {

/**
 * @brief Обработчик команд на создание/отмену ордеров
 * 
 * Слушает события из trading.events exchange:
 * - order.create → создаёт ордер через FakeBrokerAdapter
 * - order.cancel → отменяет ордер
 * 
 * Публикует результаты в broker.events exchange:
 * - order.created → ордер принят в обработку
 * - order.filled → ордер исполнен
 * - order.partially_filled → частичное исполнение
 * - order.rejected → ордер отклонён
 * - order.cancelled → ордер отменён
 */
class OrderCommandHandler {
public:
    OrderCommandHandler(
        std::shared_ptr<ports::output::IEventConsumer> eventConsumer,
        std::shared_ptr<ports::output::IEventPublisher> eventPublisher,
        std::shared_ptr<ports::output::IBrokerGateway> brokerGateway
    ) : eventConsumer_(std::move(eventConsumer))
      , eventPublisher_(std::move(eventPublisher))
      , brokerGateway_(std::move(brokerGateway))
    {
        std::cout << "[OrderCommandHandler] Created" << std::endl;
        subscribe();
    }

private:
    void subscribe() {
        std::cout << "[OrderCommandHandler] Subscribing to order.create, order.cancel" << std::endl;
        
        eventConsumer_->subscribe(
            {"order.create", "order.cancel"},
            [this](const std::string& routingKey, const std::string& message) {
                handleCommand(routingKey, message);
            }
        );
    }

    void handleCommand(const std::string& routingKey, const std::string& message) {
        std::cout << "[OrderCommandHandler] Received " << routingKey << std::endl;
        
        try {
            auto json = nlohmann::json::parse(message);
            
            if (routingKey == "order.create") {
                handleCreateOrder(json);
            } else if (routingKey == "order.cancel") {
                handleCancelOrder(json);
            }
        } catch (const std::exception& e) {
            std::cerr << "[OrderCommandHandler] Error: " << e.what() << std::endl;
        }
    }

    void handleCreateOrder(const nlohmann::json& json) {
        std::string orderId = json.value("order_id", "");
        std::string accountId = json.value("account_id", "");
        std::string figi = json.value("figi", "");
        int64_t quantity = json.value("quantity", 0);
        std::string directionStr = json.value("direction", "BUY");
        std::string typeStr = json.value("type", "MARKET");
        double price = json.value("price", 0.0);
        std::string currency = json.value("currency", "RUB");
        
        // Валидация обязательных полей
        if (orderId.empty()) {
            std::cerr << "[OrderCommandHandler] Rejected: missing order_id" << std::endl;
            publishOrderRejected("unknown", accountId, figi, "Missing required field: order_id");
            return;
        }
        if (accountId.empty()) {
            std::cerr << "[OrderCommandHandler] Rejected: missing account_id" << std::endl;
            publishOrderRejected(orderId, "", figi, "Missing required field: account_id");
            return;
        }
        if (figi.empty()) {
            std::cerr << "[OrderCommandHandler] Rejected: missing figi" << std::endl;
            publishOrderRejected(orderId, accountId, "", "Missing required field: figi");
            return;
        }
        if (quantity <= 0) {
            std::cerr << "[OrderCommandHandler] Rejected: invalid quantity" << std::endl;
            publishOrderRejected(orderId, accountId, figi, "Invalid quantity: must be > 0");
            return;
        }
        
        std::cout << "[OrderCommandHandler] Creating order " << orderId << std::endl;
        
        domain::OrderDirection direction = (directionStr == "SELL") 
            ? domain::OrderDirection::SELL 
            : domain::OrderDirection::BUY;
        
        domain::OrderType type = (typeStr == "LIMIT") 
            ? domain::OrderType::LIMIT 
            : domain::OrderType::MARKET;
        
        // Создаём request с orderId от trading-service
        domain::OrderRequest request;
        request.orderId = orderId;  // ВАЖНО: передаём orderId!
        request.figi = figi;
        request.quantity = quantity;
        request.direction = direction;
        request.type = type;
        request.price = domain::Money::fromDouble(price, currency);
        
        // Публикуем order.created
        publishOrderCreated(orderId, accountId, figi);
        
        // Исполняем (FakeBrokerAdapter использует request.orderId)
        auto result = brokerGateway_->placeOrder(accountId, request);
        
        // Публикуем результат
        if (result.status == domain::OrderStatus::FILLED) {
            publishOrderFilled(result, accountId, figi);
        } else if (result.status == domain::OrderStatus::PARTIALLY_FILLED) {
            publishOrderPartiallyFilled(result, accountId, figi);
        } else if (result.status == domain::OrderStatus::REJECTED) {
            publishOrderRejected(orderId, accountId, figi, result.message);
        }
    }

    void handleCancelOrder(const nlohmann::json& json) {
        std::string orderId = json.value("order_id", "");
        std::string accountId = json.value("account_id", "");
        
        bool cancelled = brokerGateway_->cancelOrder(accountId, orderId);
        
        if (cancelled) {
            publishOrderCancelled(orderId, accountId);
        }
    }

    void publishOrderCreated(const std::string& orderId, const std::string& accountId, const std::string& figi) {
        nlohmann::json event;
        event["order_id"] = orderId;
        event["account_id"] = accountId;
        event["figi"] = figi;
        event["status"] = "PENDING";
        event["timestamp"] = getCurrentTimestamp();
        eventPublisher_->publish("order.created", event.dump());
    }

    void publishOrderFilled(const domain::OrderResult& result, const std::string& accountId, const std::string& figi) {
        std::cout << "[OrderCommandHandler] FILLED order=" << result.orderId 
                  << " account=" << accountId 
                  << " figi=" << figi 
                  << " lots=" << result.executedLots 
                  << " price=" << result.executedPrice.toDouble() << std::endl;
        
        nlohmann::json event;
        event["order_id"] = result.orderId;
        event["account_id"] = accountId;
        event["figi"] = figi;
        event["status"] = "FILLED";
        event["executed_lots"] = result.executedLots;
        event["executed_price"] = result.executedPrice.toDouble();
        event["currency"] = result.executedPrice.currency;
        event["timestamp"] = getCurrentTimestamp();
        eventPublisher_->publish("order.filled", event.dump());
    }

    void publishOrderPartiallyFilled(const domain::OrderResult& result, const std::string& accountId, const std::string& figi) {
        nlohmann::json event;
        event["order_id"] = result.orderId;
        event["account_id"] = accountId;
        event["figi"] = figi;
        event["status"] = "PARTIALLY_FILLED";
        event["filled_lots"] = result.executedLots;
        event["executed_price"] = result.executedPrice.toDouble();
        event["timestamp"] = getCurrentTimestamp();
        eventPublisher_->publish("order.partially_filled", event.dump());
    }

    void publishOrderRejected(const std::string& orderId, const std::string& accountId, const std::string& figi, const std::string& reason) {
        std::cout << "[OrderCommandHandler] REJECTED order=" << orderId 
                  << " account=" << accountId 
                  << " figi=" << figi 
                  << " reason=" << reason << std::endl;
        
        nlohmann::json event;
        event["order_id"] = orderId;
        event["account_id"] = accountId;
        event["figi"] = figi;
        event["status"] = "REJECTED";
        event["reason"] = reason;
        event["timestamp"] = getCurrentTimestamp();
        eventPublisher_->publish("order.rejected", event.dump());
    }

    void publishOrderCancelled(const std::string& orderId, const std::string& accountId) {
        nlohmann::json event;
        event["order_id"] = orderId;
        event["account_id"] = accountId;
        event["status"] = "CANCELLED";
        event["timestamp"] = getCurrentTimestamp();
        eventPublisher_->publish("order.cancelled", event.dump());
    }

    int64_t getCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::shared_ptr<ports::output::IEventConsumer> eventConsumer_;
    std::shared_ptr<ports::output::IEventPublisher> eventPublisher_;
    std::shared_ptr<ports::output::IBrokerGateway> brokerGateway_;
};

} // namespace broker::application
