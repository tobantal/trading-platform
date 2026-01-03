// include/ports/output/IEventConsumer.hpp
#pragma once

#include <string>
#include <vector>
#include <functional>

namespace trading::ports::output {

/**
 * @brief Тип обработчика событий
 * 
 * Принимает routingKey и message как строки.
 * 
 * @param routingKey Ключ маршрутизации события
 * @param message JSON-сообщение с данными события
 */
using EventHandler = std::function<void(const std::string& routingKey, const std::string& message)>;

/**
 * @brief Интерфейс потребителя событий
 * 
 * Использует строковый интерфейс для совместимости с RabbitMQ.
 * 
 * @example
 * ```cpp
 * eventConsumer->subscribe({"order.created", "order.rejected"}, 
 *     [this](const std::string& routingKey, const std::string& message) {
 *         if (routingKey == "order.created") {
 *             handleOrderCreated(message);
 *         } else if (routingKey == "order.rejected") {
 *             handleOrderRejected(message);
 *         }
 *     });
 * eventConsumer->start();
 * ```
 */
class IEventConsumer {
public:
    virtual ~IEventConsumer() = default;
    
    /**
     * @brief Подписаться на события
     * @param routingKeys Список ключей маршрутизации для подписки
     * @param handler Обработчик событий
     */
    virtual void subscribe(const std::vector<std::string>& routingKeys, EventHandler handler) = 0;
    
    /**
     * @brief Запустить прослушивание событий
     */
    virtual void start() = 0;
    
    /**
     * @brief Остановить прослушивание
     */
    virtual void stop() = 0;
};

} // namespace trading::ports::output
