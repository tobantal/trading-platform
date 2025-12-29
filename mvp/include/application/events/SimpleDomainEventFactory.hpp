// include/application/events/SimpleDomainEventFactory.hpp
#pragma once

#include "domain/events/DomainEventFactory.hpp"
#include "domain/events/OrderCreatedEvent.hpp"
#include "domain/events/OrderFilledEvent.hpp"
#include "domain/events/OrderCancelledEvent.hpp"
#include "domain/events/QuoteUpdatedEvent.hpp"
#include "domain/events/StrategySignalEvent.hpp"
#include "domain/events/StrategyStartedEvent.hpp"
#include "domain/events/StrategyStoppedEvent.hpp"
#include <unordered_map>
#include <functional>
#include <iostream>

namespace trading::application {

/**
 * @brief Фабрика доменных событий с авторегистрацией
 * 
 * Все события регистрируются автоматически в конструкторе.
 * При добавлении нового события — добавить в registerAllEvents().
 */
class SimpleDomainEventFactory final : public domain::DomainEventFactory {
public:
    using Creator = std::function<std::unique_ptr<domain::DomainEvent>(const std::string&)>;

    /**
     * @brief Конструктор с авторегистрацией всех событий
     */
    SimpleDomainEventFactory() {
        registerAllEvents();
    }

    /**
     * @brief Создать событие по типу и JSON
     */
    std::unique_ptr<trading::domain::DomainEvent> create(
        const std::string& eventType,
        const std::string& payloadJson
    ) const override {
        auto it = creators_.find(eventType);
        if (it == creators_.end()) {
            throw std::runtime_error("Unknown domain event type: " + eventType);
        }
        return it->second(payloadJson);
    }

    /**
     * @brief Ручная регистрация события (для расширения)
     */
    void registerEvent(const std::string& eventType, Creator creator) {
        creators_[eventType] = std::move(creator);
    }

private:
    /**
     * @brief Регистрация всех доменных событий
     * 
     * @note При добавлении нового события:
     *   1. Добавить #include для нового события
     *   2. Добавить registerEvent() здесь
     */
    void registerAllEvents() {
        // ============================================
        // ORDER EVENTS
        // ============================================
        registerEvent("order.created", [](const std::string& json) {
            return std::make_unique<domain::OrderCreatedEvent>(json);
        });
        
        registerEvent("order.filled", [](const std::string& json) {
            return std::make_unique<domain::OrderFilledEvent>(json);
        });
        
        registerEvent("order.cancelled", [](const std::string& json) {
            return std::make_unique<domain::OrderCancelledEvent>(json);
        });
        
        // ============================================
        // QUOTE EVENTS
        // ============================================
        registerEvent("quote.updated", [](const std::string& json) {
            return std::make_unique<domain::QuoteUpdatedEvent>(json);
        });
        
        // ============================================
        // STRATEGY EVENTS
        // ============================================
        registerEvent("strategy.signal", [](const std::string& json) {
            return std::make_unique<domain::StrategySignalEvent>(json);
        });
        
        registerEvent("strategy.started", [](const std::string& json) {
            return std::make_unique<domain::StrategyStartedEvent>(json);
        });
        
        registerEvent("strategy.stopped", [](const std::string& json) {
            return std::make_unique<domain::StrategyStoppedEvent>(json);
        });
        
        std::cout << "[SimpleDomainEventFactory] Registered " 
                  << creators_.size() << " domain events" << std::endl;
    }

    std::unordered_map<std::string, Creator> creators_;
};

} // namespace trading::application
