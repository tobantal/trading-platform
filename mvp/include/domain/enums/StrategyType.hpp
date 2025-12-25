#pragma once

#include <string>
#include <stdexcept>

namespace trading::domain {

/**
 * @brief Тип торговой стратегии
 */
enum class StrategyType {
    SMA_CROSSOVER   ///< Пересечение скользящих средних (Simple Moving Average)
};

/**
 * @brief Преобразовать в строку
 */
inline std::string toString(StrategyType type) {
    switch (type) {
        case StrategyType::SMA_CROSSOVER: return "SMA_CROSSOVER";
    }
    return "UNKNOWN";
}

/**
 * @brief Создать из строки
 * @throws std::invalid_argument если строка не распознана
 */
inline StrategyType strategyTypeFromString(const std::string& str) {
    if (str == "SMA_CROSSOVER") return StrategyType::SMA_CROSSOVER;
    throw std::invalid_argument("Unknown StrategyType: " + str);
}

/**
 * @brief Получить человекочитаемое название стратегии
 */
inline std::string getDisplayName(StrategyType type) {
    switch (type) {
        case StrategyType::SMA_CROSSOVER: return "SMA Crossover";
    }
    return "Unknown Strategy";
}

/**
 * @brief Получить описание стратегии
 */
inline std::string getDescription(StrategyType type) {
    switch (type) {
        case StrategyType::SMA_CROSSOVER: 
            return "Торговля на пересечении короткой и длинной скользящих средних";
    }
    return "";
}

} // namespace trading::domain
