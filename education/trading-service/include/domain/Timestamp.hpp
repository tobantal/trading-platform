#pragma once

#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace trading::domain {

/**
 * @brief Временная метка
 */
class Timestamp {
public:
    std::chrono::system_clock::time_point value;

    Timestamp() : value(std::chrono::system_clock::now()) {}
    
    explicit Timestamp(std::chrono::system_clock::time_point tp) : value(tp) {}

    static Timestamp now() {
        return Timestamp(std::chrono::system_clock::now());
    }

    static Timestamp fromString(const std::string& str) {
        // Простой парсинг ISO 8601
        std::tm tm = {};
        std::istringstream ss(str);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        return Timestamp(tp);
    }

    std::string toString() const {
        auto time_t_val = std::chrono::system_clock::to_time_t(value);
        std::tm tm = *std::gmtime(&time_t_val);
        
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }

    bool operator<(const Timestamp& other) const {
        return value < other.value;
    }

    bool operator>(const Timestamp& other) const {
        return value > other.value;
    }
};

} // namespace trading::domain
