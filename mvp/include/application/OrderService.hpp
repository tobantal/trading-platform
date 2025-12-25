#pragma once

#include "ports/input/IOrderService.hpp"
#include "ports/output/IBrokerGateway.hpp"
#include "ports/output/IOrderRepository.hpp"
#include "ports/output/IEventBus.hpp"
#include "domain/events/OrderCreatedEvent.hpp"
#include "domain/events/OrderFilledEvent.hpp"
#include "domain/events/OrderCancelledEvent.hpp"
#include <memory>
#include <random>
#include <sstream>
#include <iomanip>

namespace trading::application {

/**
 * @brief Сервис управления ордерами
 * 
 * Реализует IOrderService, координирует работу между:
 * - IBrokerGateway (размещение ордеров у брокера)
 * - IOrderRepository (сохранение истории ордеров)
 * - IEventBus (публикация событий)
 */
class OrderService : public ports::input::IOrderService {
public:
    OrderService(
        std::shared_ptr<ports::output::IBrokerGateway> broker,
        std::shared_ptr<ports::output::IOrderRepository> orderRepository,
        std::shared_ptr<ports::output::IEventBus> eventBus
    ) : broker_(std::move(broker))
      , orderRepository_(std::move(orderRepository))
      , eventBus_(std::move(eventBus))
      , rng_(std::random_device{}())
    {}

    /**
     * @brief Создать и разместить ордер
     */
    domain::OrderResult placeOrder(const domain::OrderRequest& request) override {
        // Размещаем ордер у брокера
        auto result = broker_->placeOrder(request);

        if (result.status != domain::OrderStatus::REJECTED) {
            // Создаём объект ордера для сохранения
            domain::Order order(
                result.orderId,
                request.accountId,
                request.figi,
                request.direction,
                request.type,
                request.quantity,
                result.executedPrice.toDouble() > 0 ? result.executedPrice : request.price
            );
            order.updateStatus(result.status);

            // Сохраняем в репозиторий
            orderRepository_->save(order);

            // Публикуем событие создания
            publishOrderCreatedEvent(order);

            // Если исполнен - публикуем событие исполнения
            if (result.status == domain::OrderStatus::FILLED) {
                publishOrderFilledEvent(order, result.executedPrice);
            }
        }

        return result;
    }

    /**
     * @brief Отменить ордер
     */
    bool cancelOrder(const std::string& accountId, const std::string& orderId) override {
        // Проверяем, что ордер существует и принадлежит счёту
        auto order = orderRepository_->findById(orderId);
        if (!order || order->accountId != accountId) {
            return false;
        }

        // Можно отменить только PENDING ордер
        if (order->status != domain::OrderStatus::PENDING) {
            return false;
        }

        // Отменяем у брокера
        bool cancelled = broker_->cancelOrder(orderId);
        if (cancelled) {
            orderRepository_->updateStatus(orderId, domain::OrderStatus::CANCELLED);
            publishOrderCancelledEvent(*order);
        }

        return cancelled;
    }

    /**
     * @brief Получить ордер по ID
     */
    std::optional<domain::Order> getOrderById(const std::string& orderId) override {
        return orderRepository_->findById(orderId);
    }

    /**
     * @brief Получить активные ордера счёта
     */
    std::vector<domain::Order> getActiveOrders(const std::string& accountId) override {
        return orderRepository_->findByStatus(accountId, domain::OrderStatus::PENDING);
    }

    /**
     * @brief Получить историю ордеров за период
     */
    std::vector<domain::Order> getOrderHistory(
        const std::string& accountId,
        const domain::Timestamp& from,
        const domain::Timestamp& to
    ) override {
        return orderRepository_->findByPeriod(accountId, from, to);
    }

    /**
     * @brief Получить все ордера счёта
     */
    std::vector<domain::Order> getAllOrders(const std::string& accountId) override {
        return orderRepository_->findByAccountId(accountId);
    }

private:
    std::shared_ptr<ports::output::IBrokerGateway> broker_;
    std::shared_ptr<ports::output::IOrderRepository> orderRepository_;
    std::shared_ptr<ports::output::IEventBus> eventBus_;
    std::mt19937_64 rng_;

    void publishOrderCreatedEvent(const domain::Order& order) {
        domain::OrderCreatedEvent event;
        event.orderId = order.id;
        event.accountId = order.accountId;
        event.figi = order.figi;
        event.direction = order.direction;
        event.orderType = order.type;
        event.quantity = order.quantity;
        event.price = order.price;
        eventBus_->publish(event);
    }

    void publishOrderFilledEvent(const domain::Order& order, const domain::Money& executedPrice) {
        domain::OrderFilledEvent event;
        event.orderId = order.id;
        event.accountId = order.accountId;
        event.figi = order.figi;
        event.executedPrice = executedPrice;
        event.quantity = order.quantity;
        eventBus_->publish(event);
    }

    void publishOrderCancelledEvent(const domain::Order& order) {
        domain::OrderCancelledEvent event;
        event.orderId = order.id;
        event.accountId = order.accountId;
        eventBus_->publish(event);
    }
};

} // namespace trading::application
