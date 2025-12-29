// include/domain/events/DomainEventFactory.hpp
#pragma once

#include <memory>
#include <string>

#include "domain/events/DomainEvent.hpp"

namespace trading::domain {

class DomainEventFactory {
public:
    virtual ~DomainEventFactory() = default;

    virtual std::unique_ptr<DomainEvent> create(
        const std::string& eventType,
        const std::string& payloadJson
    ) const = 0;
};

} // namespace trading::domain
