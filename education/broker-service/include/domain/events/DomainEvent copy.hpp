#pragma once

#include "domain/Timestamp.hpp"
#include <string>
#include <memory>

namespace trading::domain {

/**
 * @brief Базовый класс для всех доменных событий
 * 
 * Используется Event Bus для передачи событий между компонентами.
 */
struct DomainEvent {
    std::string eventId;        ///< UUID события
    std::string eventType;      ///< Тип события (order.created, strategy.signal)
    Timestamp timestamp;        ///< Время создания события

    DomainEvent() : timestamp(Timestamp::now()) {}
    
    explicit DomainEvent(const std::string& type)
        : eventType(type), timestamp(Timestamp::now()) {}

    virtual ~DomainEvent() = default;

    /**
     * @brief Сериализовать в JSON
     */
    virtual std::string toJson() const = 0;

    /**
     * @brief Клонировать событие
     */
    virtual std::unique_ptr<DomainEvent> clone() const = 0;
};

} // namespace trading::domain