#pragma once

#include <cstdlib>
#include <string>

namespace trading::settings {

/**
 * @brief Настройки кэширования
 * 
 * Читает из ENV:
 * - CACHE_QUOTE_SIZE (default: 1000)
 * - CACHE_QUOTE_TTL_SECONDS (default: 10)
 * - CACHE_INSTRUMENT_SIZE (default: 500)
 * - CACHE_INSTRUMENT_TTL_SECONDS (default: 3600)
 */
class CacheSettings {
public:
    CacheSettings() {
        if (const char* val = std::getenv("CACHE_QUOTE_SIZE")) {
            quoteCacheSize_ = static_cast<size_t>(std::stoi(val));
        }
        if (const char* val = std::getenv("CACHE_QUOTE_TTL_SECONDS")) {
            quoteTtlSeconds_ = std::stoi(val);
        }
        if (const char* val = std::getenv("CACHE_INSTRUMENT_SIZE")) {
            instrumentCacheSize_ = static_cast<size_t>(std::stoi(val));
        }
        if (const char* val = std::getenv("CACHE_INSTRUMENT_TTL_SECONDS")) {
            instrumentTtlSeconds_ = std::stoi(val);
        }
    }
    
    size_t getQuoteCacheSize() const { return quoteCacheSize_; }
    int getQuoteTtlSeconds() const { return quoteTtlSeconds_; }
    size_t getInstrumentCacheSize() const { return instrumentCacheSize_; }
    int getInstrumentTtlSeconds() const { return instrumentTtlSeconds_; }

private:
    size_t quoteCacheSize_ = 1000;
    int quoteTtlSeconds_ = 10;
    size_t instrumentCacheSize_ = 500;
    int instrumentTtlSeconds_ = 3600;
};

} // namespace trading::settings
