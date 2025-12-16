#pragma once

#include <string>
#include <stdexcept>

namespace trading::domain {

/**
 * @brief Тип торгового сигнала от стратегии
 */
enum class SignalType {
    BUY,    ///< Сигнал на покупку
    SELL,   ///< Сигнал на продажу
    HOLD    ///< Удерживать позицию (ничего не делать)
};

/**
 * @brief Преобразовать в строку
 */
inline std::string toString(SignalType type) {
    switch (type) {
        case SignalType::BUY:  return "BUY";
        case SignalType::SELL: return "SELL";
        case SignalType::HOLD: return "HOLD";
    }
    return "UNKNOWN";
}

/**
 * @brief Создать из строки
 * @throws std::invalid_argument если строка не распознана
 */
inline SignalType signalTypeFromString(const std::string& str) {
    if (str == "BUY")  return SignalType::BUY;
    if (str == "SELL") return SignalType::SELL;
    if (str == "HOLD") return SignalType::HOLD;
    throw std::invalid_argument("Unknown SignalType: " + str);
}

/**
 * @brief Требует ли сигнал действия (не HOLD)
 */
inline bool requiresAction(SignalType type) {
    return type != SignalType::HOLD;
}

/**
 * @brief Это сигнал на покупку?
 */
inline bool isBuySignal(SignalType type) {
    return type == SignalType::BUY;
}

/**
 * @brief Это сигнал на продажу?
 */
inline bool isSellSignal(SignalType type) {
    return type == SignalType::SELL;
}

/**
 * @brief Получить противоположный сигнал
 * @note HOLD возвращает HOLD
 */
inline SignalType opposite(SignalType type) {
    switch (type) {
        case SignalType::BUY:  return SignalType::SELL;
        case SignalType::SELL: return SignalType::BUY;
        case SignalType::HOLD: return SignalType::HOLD;
    }
    return SignalType::HOLD;
}

} // namespace trading::domain