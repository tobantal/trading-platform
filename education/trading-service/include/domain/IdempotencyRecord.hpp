#pragma once

#include <string>
#include <cstdint>

/**
 * Идемпотентность запросов
 */
namespace trading::domain
{

    struct IdempotencyRecord
    {
        std::string key;
        int status;
        std::string body;
    };

} // namespace trading::domain
