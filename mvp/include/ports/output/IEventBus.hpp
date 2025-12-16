#pragma once

#include "domain/events/DomainEvent.hpp"
#include <string>
#include <functional>
#include <memory>

namespace trading::ports::output {

/**
 * @brief Callback для обработчиков событий
 */
using EventHandler = std::function<void(const domain::DomainEvent&)>;

/**
 * @brief Интерфейс событийной шины
 * 
 * Output Port для публикации и подписки на доменные события.
 * Обеспечивает слабую связанность между компонентами системы.
 * 
 * Реализации:
 * - InMemoryEventBus (MVP) - события в памяти
 * - RabbitMqEventBus (Education) - RabbitMQ
 */
class IEventBus {
public:
    virtual ~IEventBus() = default;

    /**
     * @brief Опубликовать событие
     * 
     * @param event Доменное событие
     * 
     * @note Событие асинхронно доставляется всем подписчикам
     */
    virtual void publish(const domain::DomainEvent& event) = 0;

    /**
     * @brief Подписаться на тип события
     * 
     * @param eventType Тип события (например, "order.created")
     * @param handler Функция-обработчик
     * 
     * @note Один eventType может иметь несколько handlers
     */
    virtual void subscribe(const std::string& eventType, EventHandler handler) = 0;

    /**
     * @brief Отписаться от типа события
     * 
     * @param eventType Тип события
     * 
     * @note Удаляет ВСЕ handlers для данного eventType
     */
    virtual void unsubscribe(const std::string& eventType) = 0;

    /**
     * @brief Проверить наличие подписчиков
     * 
     * @param eventType Тип события
     * @return true если есть хотя бы один подписчик
     */
    virtual bool hasSubscribers(const std::string& eventType) const = 0;

    /**
     * @brief Запустить обработку событий
     * 
     * @note Для асинхронных реализаций запускает worker thread
     */
    virtual void start() = 0;

    /**
     * @brief Остановить обработку событий
     * 
     * @note Graceful shutdown - дожидается обработки текущих событий
     */
    virtual void stop() = 0;
};

} // namespace trading::ports::output