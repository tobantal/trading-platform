#pragma once

#include <string>

namespace trading::ports::output {

/**
 * @brief Интерфейс для публикации событий
 * 
 * Реализуется RabbitMQAdapter.
 */
class IEventPublisher {
public:
    virtual ~IEventPublisher() = default;

    /**
     * @brief Опубликовать событие
     * @param routingKey Ключ маршрутизации (например, "order.create")
     * @param message JSON-сообщение FIXME: заменить на UObject
     */
    virtual void publish(const std::string& routingKey, const std::string& message) = 0;
};

} // namespace trading::ports::output
