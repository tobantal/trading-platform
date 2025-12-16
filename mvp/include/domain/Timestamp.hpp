#pragma once

#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace trading::domain {

/**
 * @brief Временная метка в ISO 8601 формате
 */
struct Timestamp {
    std::chrono::system_clock::time_point value;

    Timestamp() : value(std::chrono::system_clock::now()) {}
    
    explicit Timestamp(std::chrono::system_clock::time_point tp) : value(tp) {}

    /**
     * @brief Создать Timestamp с текущим временем
     */
    static Timestamp now() {
        return Timestamp(std::chrono::system_clock::now());
    }

    /**
     * @brief Создать Timestamp из ISO 8601 строки
     * @param isoString Строка формата "2025-12-16T10:30:00Z"
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
     * @brief Преобразовать в ISO 8601 строку
     */
    std::string toString() const {
        auto time_t_val = std::chrono::system_clock::to_time_t(value);
        std::tm tm = *std::gmtime(&time_t_val);
        
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }

    /**
     * @brief Получить Unix timestamp (секунды с 1970)
     */
    int64_t toUnixSeconds() const {
        return std::chrono::duration_cast<std::chrono::seconds>(
            value.time_since_epoch()
        ).count();
    }

    /**
     * @brief Создать из Unix timestamp
     */
    static Timestamp fromUnixSeconds(int64_t seconds) {
        return Timestamp(std::chrono::system_clock::time_point(
            std::chrono::seconds(seconds)
        ));
    }

    /**
     * @brief Добавить секунды
     */
    Timestamp addSeconds(int64_t seconds) const {
        return Timestamp(value + std::chrono::seconds(seconds));
    }

    /**
     * @brief Добавить минуты
     */
    Timestamp addMinutes(int64_t minutes) const {
        return Timestamp(value + std::chrono::minutes(minutes));
    }

    /**
     * @brief Добавить часы
     */
    Timestamp addHours(int64_t hours) const {
        return Timestamp(value + std::chrono::hours(hours));
    }

    // Операторы сравнения
    bool operator==(const Timestamp& other) const { return value == other.value; }
    bool operator!=(const Timestamp& other) const { return value != other.value; }
    bool operator<(const Timestamp& other) const { return value < other.value; }
    bool operator>(const Timestamp& other) const { return value > other.value; }
    bool operator<=(const Timestamp& other) const { return value <= other.value; }
    bool operator>=(const Timestamp& other) const { return value >= other.value; }
};

} // namespace trading::domain