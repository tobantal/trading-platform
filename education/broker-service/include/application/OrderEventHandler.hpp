#pragma once

#include "ports/output/IEventConsumer.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace broker::application {

/**
 * @brief Обработчик событий ордеров
 * 
 * Подписывается на события order.filled и order.rejected
 * и обрабатывает их (логирование, обновление состояния и т.д.)
 */
class OrderEventHandler {
public:
    explicit OrderEventHandler(std::shared_ptr<ports::output::IEventConsumer> eventConsumer)
        : eventConsumer_(std::move(eventConsumer))
    {
        subscribe();
    }

private:
    void subscribe() {
        // Подписываемся на события ордеров
        eventConsumer_->subscribe(
            {"order.filled", "order.rejected"},
            [this](const std::string& routingKey, const std::string& message) {
                handleOrderEvent(routingKey, message);
            }
        );
    }

    void handleOrderEvent(const std::string& routingKey, const std::string& message) {
        try {
            auto json = nlohmann::json::parse(message);
            
            if (routingKey == "order.filled") {
                std::cout << "[OrderEventHandler] Order filled: " 
                          << json.value("order_id", "unknown") 
                          << ", price: " << json.value("executed_price", 0.0)
                          << std::endl;
            } else if (routingKey == "order.rejected") {
                std::cout << "[OrderEventHandler] Order rejected: " 
                          << json.value("order_id", "unknown")
                          << ", reason: " << json.value("reason", "unknown")
                          << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[OrderEventHandler] Failed to parse event: " << e.what() << std::endl;
        }
    }

    std::shared_ptr<ports::output::IEventConsumer> eventConsumer_;
};

} // namespace broker::application
