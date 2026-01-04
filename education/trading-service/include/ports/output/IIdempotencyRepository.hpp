#pragma once

#include "domain/IdempotencyRecord.hpp"
#include <string>
#include <optional>

namespace trading::ports::output {

class IIdempotencyRepository {
public:
    virtual ~IIdempotencyRepository() = default;
    virtual std::optional<domain::IdempotencyRecord> find(const std::string& key) = 0;
    virtual void save(const std::string& key, int status, const std::string& body) = 0;
};

} // namespace trading::ports::output
