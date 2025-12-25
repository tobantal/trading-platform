#pragma once

#include <string>
#include <stdexcept>

namespace trading::domain {

/**
 * @brief Статус торговой стратегии
 */
enum class StrategyStatus {
    STOPPED,    ///< Остановлена
    RUNNING,    ///< Запущена и работает
    ERROR       ///< Ошибка выполнения
};

/**
 * @brief Преобразовать в строку
 */
inline std::string toString(StrategyStatus status) {
    switch (status) {
        case StrategyStatus::STOPPED: return "STOPPED";
        case StrategyStatus::RUNNING: return "RUNNING";
        case StrategyStatus::ERROR:   return "ERROR";
    }
    return "UNKNOWN";
}

/**
 * @brief Создать из строки
 * @throws std::invalid_argument если строка не распознана
 */
inline StrategyStatus strategyStatusFromString(const std::string& str) {
    if (str == "STOPPED") return StrategyStatus::STOPPED;
    if (str == "RUNNING") return StrategyStatus::RUNNING;
    if (str == "ERROR")   return StrategyStatus::ERROR;
    throw std::invalid_argument("Unknown StrategyStatus: " + str);
}

/**
 * @brief Можно ли запустить стратегию в данном статусе
 */
inline bool canStart(StrategyStatus status) {
    return status == StrategyStatus::STOPPED || status == StrategyStatus::ERROR;
}

/**
 * @brief Можно ли остановить стратегию в данном статусе
 */
inline bool canStop(StrategyStatus status) {
    return status == StrategyStatus::RUNNING;
}

/**
 * @brief Активна ли стратегия (генерирует сигналы)
 */
inline bool isActive(StrategyStatus status) {
    return status == StrategyStatus::RUNNING;
}

} // namespace trading::domain
