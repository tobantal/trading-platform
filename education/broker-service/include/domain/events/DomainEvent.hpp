#pragma once

#include "domain/Timestamp.hpp"
#include <string>
#include <memory>

namespace broker::domain {

/**
 * @brief Ð‘Ð°Ð·Ð¾Ð²Ñ‹Ð¹ ÐºÐ»Ð°ÑÑ Ð´Ð»Ñ Ð²ÑÐµÑ… Ð´Ð¾Ð¼ÐµÐ½Ð½Ñ‹Ñ… ÑÐ¾Ð±Ñ‹Ñ‚Ð¸Ð¹
 * 
 * Ð˜ÑÐ¿Ð¾Ð»ÑŒÐ·ÑƒÐµÑ‚ÑÑ Event Bus Ð´Ð»Ñ Ð¿ÐµÑ€ÐµÐ´Ð°Ñ‡Ð¸ ÑÐ¾Ð±Ñ‹Ñ‚Ð¸Ð¹ Ð¼ÐµÐ¶Ð´Ñƒ ÐºÐ¾Ð¼Ð¿Ð¾Ð½ÐµÐ½Ñ‚Ð°Ð¼Ð¸.
 */
struct DomainEvent {
    std::string eventId;        ///< UUID ÑÐ¾Ð±Ñ‹Ñ‚Ð¸Ñ
    std::string eventType;      ///< Ð¢Ð¸Ð¿ ÑÐ¾Ð±Ñ‹Ñ‚Ð¸Ñ (order.created, strategy.signal)
    Timestamp timestamp;        ///< Ð’Ñ€ÐµÐ¼Ñ ÑÐ¾Ð·Ð´Ð°Ð½Ð¸Ñ ÑÐ¾Ð±Ñ‹Ñ‚Ð¸Ñ

    DomainEvent() : timestamp(Timestamp::now()) {}
    
    explicit DomainEvent(const std::string& type)
        : eventType(type), timestamp(Timestamp::now()) {}

    virtual ~DomainEvent() = default;

    /**
     * @brief Ð¡ÐµÑ€Ð¸Ð°Ð»Ð¸Ð·Ð¾Ð²Ð°Ñ‚ÑŒ Ð² JSON
     */
    virtual std::string toJson() const = 0;

    /**
     * @brief ÐšÐ»Ð¾Ð½Ð¸Ñ€Ð¾Ð²Ð°Ñ‚ÑŒ ÑÐ¾Ð±Ñ‹Ñ‚Ð¸Ðµ
     */
    virtual std::unique_ptr<DomainEvent> clone() const = 0;
};

} // namespace broker::domain
