// include/domain/Timestamp.hpp
#pragma once

#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace broker::domain {

/**
 * @brief Timestamp in ISO 8601 format
 */
struct Timestamp {
    std::chrono::system_clock::time_point value;

    Timestamp() : value(std::chrono::system_clock::now()) {}
    
    explicit Timestamp(std::chrono::system_clock::time_point tp) : value(tp) {}

    /**
     * @brief Create Timestamp with current time
     */
    static Timestamp now() {
        return Timestamp(std::chrono::system_clock::now());
    }

    /**
     * @brief Create Timestamp from ISO 8601 string
     * @param isoString String format "2025-12-16T10:30:00Z"
     */
    static Timestamp fromString(const std::string& isoString) {
        std::tm tm = {};
        std::istringstream ss(isoString);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        
        if (ss.fail()) {
            return Timestamp::now();
        }
        
        auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        return Timestamp(tp);
    }

    /**
     * @brief Convert to ISO 8601 string
     */
    std::string toString() const {
        auto time_t_val = std::chrono::system_clock::to_time_t(value);
        std::tm tm = *std::gmtime(&time_t_val);
        
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }

    /**
     * @brief Get Unix timestamp (seconds since 1970)
     */
    int64_t toUnixSeconds() const {
        return std::chrono::duration_cast<std::chrono::seconds>(
            value.time_since_epoch()
        ).count();
    }

    /**
     * @brief Get Unix timestamp in milliseconds
     */
    int64_t toUnixMillis() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            value.time_since_epoch()
        ).count();
    }

    /**
     * @brief Create Timestamp from Unix seconds
     */
    static Timestamp fromUnixSeconds(int64_t seconds) {
        return Timestamp(std::chrono::system_clock::time_point(
            std::chrono::seconds(seconds)
        ));
    }

    /**
     * @brief Create Timestamp from Unix milliseconds
     */
    static Timestamp fromUnixMillis(int64_t millis) {
        return Timestamp(std::chrono::system_clock::time_point(
            std::chrono::milliseconds(millis)
        ));
    }

    bool operator<(const Timestamp& other) const { return value < other.value; }
    bool operator>(const Timestamp& other) const { return value > other.value; }
    bool operator<=(const Timestamp& other) const { return value <= other.value; }
    bool operator>=(const Timestamp& other) const { return value >= other.value; }
    bool operator==(const Timestamp& other) const { return value == other.value; }
    bool operator!=(const Timestamp& other) const { return value != other.value; }
};

} // namespace broker::domain
