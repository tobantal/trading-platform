#pragma once

#include <string>
#include <random>
#include <sstream>
#include <iomanip>

namespace trading::utils {

/**
 * @brief Генератор UUID v4
 * 
 * Централизованная утилита для генерации уникальных идентификаторов.
 * Устраняет дублирование кода генерации UUID в разных сервисах.
 * 
 * @note Thread-safe благодаря thread_local генератору
 */
class UuidGenerator {
public:
    /**
     * @brief Генерирует UUID v4
     * 
     * Формат: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
     * где x - hex digit, y - один из [8, 9, a, b]
     * 
     * @return UUID строка в стандартном формате
     */
    static std::string generate() {
        thread_local std::random_device rd;
        thread_local std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dist;

        uint64_t part1 = dist(gen);
        uint64_t part2 = dist(gen);

        std::ostringstream ss;
        ss << std::hex << std::setfill('0');
        
        // xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
        ss << std::setw(8) << ((part1 >> 32) & 0xFFFFFFFF) << "-";
        ss << std::setw(4) << ((part1 >> 16) & 0xFFFF) << "-";
        ss << std::setw(4) << ((part1 & 0x0FFF) | 0x4000) << "-";  // version 4
        ss << std::setw(4) << ((part2 >> 48) & 0x3FFF | 0x8000) << "-";  // variant
        ss << std::setw(12) << (part2 & 0xFFFFFFFFFFFF);

        return ss.str();
    }

    /**
     * @brief Генерирует короткий ID с префиксом
     * 
     * @param prefix Префикс (например, "ord", "str", "acc")
     * @return ID в формате "prefix-xxxxxxxx"
     */
    static std::string generateWithPrefix(const std::string& prefix) {
        thread_local std::random_device rd;
        thread_local std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint32_t> dist;

        std::ostringstream ss;
        ss << prefix << "-" << std::hex << std::setfill('0') << std::setw(8) << dist(gen);
        return ss.str();
    }
};

} // namespace trading::utils
