#pragma once

#include "domain/Timestamp.hpp"
#include <string>
#include <memory>

namespace broker::domain
{

    /**
     * Базовый тип события
     */
    struct DomainEvent
    {
        std::string eventId;   ///< UUID события
        std::string eventType; ///< тип события (order.created, strategy.signal)
        Timestamp timestamp;   ///< время создания

        DomainEvent() : timestamp(Timestamp::now()) {}

        explicit DomainEvent(const std::string &type)
            : eventType(type), timestamp(Timestamp::now()) {}

        virtual ~DomainEvent() = default;

        /**
         * @brief конвертировать в строку-json
         */
        virtual std::string toJson() const = 0;

        /**
         * @brief клонировать
         */
        virtual std::unique_ptr<DomainEvent> clone() const = 0;
    };

} // namespace broker::domain
