// trading-service/include/application/OrderService.hpp
#pragma once

#include "ports/input/IOrderService.hpp"
#include "ports/output/IBrokerGateway.hpp"
#include "ports/output/IEventPublisher.hpp"
#include "domain/OrderRequest.hpp"
#include "domain/OrderResult.hpp"
#include "domain/Order.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>

namespace trading::application {

/**
 * @brief Сервис управления ордерами
 * 
 * Архитектура:
 * - POST (создание) → валидация FIGI → публикует событие в RabbitMQ → broker слушает
 * - DELETE (отмена) → публикует событие в RabbitMQ → broker слушает
 * - GET (чтение) → HTTP запрос к broker-service
 */
class OrderService : public ports::input::IOrderService {
public:
    OrderService(
        std::shared_ptr<ports::output::IBrokerGateway> broker,
        std::shared_ptr<ports::output::IEventPublisher> eventPublisher
    ) : broker_(std::move(broker))
      , eventPublisher_(std::move(eventPublisher))
      , rng_(std::random_device{}())
    {
        std::cout << "[OrderService] Created" << std::endl;
    }

    /**
     * @brief Создать ордер (публикует в RabbitMQ)
     * 
     * Валидирует FIGI перед отправкой.
     * Возвращает OrderResult со статусом PENDING и сгенерированным orderId.
     * Реальное исполнение произойдёт асинхронно в broker-service.
     */
    domain::OrderResult placeOrder(const domain::OrderRequest& request) override {
        domain::OrderResult result;
        result.orderId = generateOrderId();
        result.timestamp = domain::Timestamp::now();

        // Валидация FIGI перед отправкой
        auto instrument = broker_->getInstrumentByFigi(request.figi);
        if (!instrument) {
            std::cout << "[OrderService] REJECTED: Invalid FIGI " << request.figi << std::endl;
            result.status = domain::OrderStatus::REJECTED;
            result.message = "Invalid FIGI: " + request.figi;
            return result;
        }

        result.status = domain::OrderStatus::PENDING;
        result.message = "Order submitted for processing";

        try {
            // Формируем событие
            nlohmann::json event;
            event["order_id"] = result.orderId;
            event["account_id"] = request.accountId;
            event["figi"] = request.figi;
            event["direction"] = domain::toString(request.direction);
            event["type"] = domain::toString(request.type);
            event["quantity"] = request.quantity;
            event["price"] = request.price.toDouble();
            event["currency"] = request.price.currency;
            event["timestamp"] = result.timestamp.toString();

            // Публикуем в RabbitMQ
            eventPublisher_->publish("order.create", event.dump());
            
            std::cout << "[OrderService] Published order.create: " << result.orderId << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "[OrderService] Failed to publish order: " << e.what() << std::endl;
            result.status = domain::OrderStatus::REJECTED;
            result.message = std::string("Failed to submit order: ") + e.what();
        }

        return result;
    }

    /**
     * @brief Отменить ордер (публикует в RabbitMQ)
     */
    bool cancelOrder(const std::string& accountId, const std::string& orderId) override {
        try {
            nlohmann::json event;
            event["order_id"] = orderId;
            event["account_id"] = accountId;
            event["timestamp"] = domain::Timestamp::now().toString();

            eventPublisher_->publish("order.cancel", event.dump());
            
            std::cout << "[OrderService] Published order.cancel: " << orderId << std::endl;
            return true;

        } catch (const std::exception& e) {
            std::cerr << "[OrderService] Failed to cancel order: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Получить ордер по ID (HTTP к broker-service)
     */
    std::optional<domain::Order> getOrderById(
        const std::string& accountId, 
        const std::string& orderId) override 
    {
        // Получаем все ордера аккаунта и ищем нужный
        auto orders = broker_->getOrders(accountId);
        
        for (const auto& order : orders) {
            if (order.id == orderId) {
                std::cout << "[OrderService] Found order " << orderId << std::endl;
                return order;
            }
        }
        
        std::cout << "[OrderService] Order not found: " << orderId << std::endl;
        return std::nullopt;
    }

    /**
     * @brief Получить все ордера аккаунта (HTTP к broker-service)
     */
    std::vector<domain::Order> getAllOrders(const std::string& accountId) override {
        return broker_->getOrders(accountId);
    }

private:
    std::shared_ptr<ports::output::IBrokerGateway> broker_;
    std::shared_ptr<ports::output::IEventPublisher> eventPublisher_;
    std::mt19937 rng_;

    std::string generateOrderId() {
        std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
        uint64_t id = dist(rng_);
        
        std::stringstream ss;
        ss << "ord-" << std::hex << std::setfill('0') << std::setw(16) << id;
        return ss.str();
    }
};

} // namespace trading::application
