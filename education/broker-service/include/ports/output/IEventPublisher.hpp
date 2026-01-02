// include/ports/output/IEventPublisher.hpp
#pragma once

#include <string>

namespace broker::ports::output {

/**
 * @brief Интерфейс издателя событий
 * 
 * Использует строковый интерфейс (routingKey + message) для совместимости
 * с RabbitMQ и другими брокерами сообщений.
 * 
 * @example
 * ```cpp
 * eventPublisher->publish("order.executed", R"({"order_id":"ord-001","status":"FILLED"})");
 * ```
 */
class IEventPublisher {
public:
    virtual ~IEventPublisher() = default;
    
    /**
     * @brief Опубликовать событие
     * @param routingKey Ключ маршрутизации (например, "order.executed", "order.rejected")
     * @param message JSON-сообщение с данными события
     */
    virtual void publish(const std::string& routingKey, const std::string& message) = 0;
};

} // namespace broker::ports::output
