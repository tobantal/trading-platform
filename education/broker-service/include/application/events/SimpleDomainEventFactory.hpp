// include/application/events/SimpleDomainEventFactory.hpp
#pragma once

#include "domain/events/DomainEventFactory.hpp"
#include "domain/events/OrderCreatedEvent.hpp"
#include "domain/events/OrderFilledEvent.hpp"
#include "domain/events/OrderCancelledEvent.hpp"
#include "domain/events/QuoteUpdatedEvent.hpp"
#include <unordered_map>
#include <functional>
#include <iostream>

namespace broker::application {

/**
 * @brief Domain event factory implementation
 */
class SimpleDomainEventFactory final : public domain::DomainEventFactory {
public:
    using Creator = std::function<std::unique_ptr<domain::DomainEvent>(const std::string&)>;

    SimpleDomainEventFactory() {
        registerAllEvents();
    }

    std::unique_ptr<broker::domain::DomainEvent> create(
        const std::string& eventType,
        const std::string& payloadJson
    ) const override {
        auto it = creators_.find(eventType);
        if (it == creators_.end()) {
            std::cerr << "[SimpleDomainEventFactory] Unknown event type: " << eventType << std::endl;
            return nullptr;
        }
        return it->second(payloadJson);
    }

    void registerEvent(const std::string& eventType, Creator creator) {
        creators_[eventType] = std::move(creator);
    }

private:
    void registerAllEvents() {
        registerEvent("order.created", [](const std::string& json) {
            return std::make_unique<domain::OrderCreatedEvent>(
                domain::OrderCreatedEvent::fromJson(json)
            );
        });

        registerEvent("order.filled", [](const std::string& json) {
            return std::make_unique<domain::OrderFilledEvent>(
                domain::OrderFilledEvent::fromJson(json)
            );
        });

        registerEvent("order.cancelled", [](const std::string& json) {
            return std::make_unique<domain::OrderCancelledEvent>(
                domain::OrderCancelledEvent::fromJson(json)
            );
        });

        registerEvent("quote.updated", [](const std::string& json) {
            return std::make_unique<domain::QuoteUpdatedEvent>(
                domain::QuoteUpdatedEvent::fromJson(json)
            );
        });
    }

    std::unordered_map<std::string, Creator> creators_;
};

} // namespace broker::application
