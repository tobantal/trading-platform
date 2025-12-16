#pragma once

#include "enums/SignalType.hpp"
#include "Money.hpp"
#include "Timestamp.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Торговый сигнал, сгенерированный стратегией
 * 
 * Фиксирует решение стратегии о покупке/продаже/удержании.
 * Используется для аудита и анализа работы стратегии.
 */
struct Signal {
    std::string id;             ///< UUID сигнала
    std::string strategyId;     ///< FK на strategies
    SignalType type;            ///< BUY / SELL / HOLD
    std::string figi;           ///< FIGI инструмента
    Money price;                ///< Цена на момент сигнала
    std::string reason;         ///< Причина ("SMA10 crossed above SMA30")
    Timestamp createdAt;        ///< Время генерации сигнала

    Signal() : type(SignalType::HOLD) {}

    Signal(
        const std::string& id,
        const std::string& strategyId,
        SignalType type,
        const std::string& figi,
        const Money& price,
        const std::string& reason
    ) : id(id), strategyId(strategyId), type(type), figi(figi),
        price(price), reason(reason), createdAt(Timestamp::now()) {}

    /**
     * @brief Создать сигнал на покупку
     */
    static Signal buy(
        const std::string& id,
        const std::string& strategyId,
        const std::string& figi,
        const Money& price,
        const std::string& reason
    ) {
        return Signal(id, strategyId, SignalType::BUY, figi, price, reason);
    }

    /**
     * @brief Создать сигнал на продажу
     */
    static Signal sell(
        const std::string& id,
        const std::string& strategyId,
        const std::string& figi,
        const Money& price,
        const std::string& reason
    ) {
        return Signal(id, strategyId, SignalType::SELL, figi, price, reason);
    }

    /**
     * @brief Создать сигнал удержания
     */
    static Signal hold(
        const std::string& id,
        const std::string& strategyId,
        const std::string& figi,
        const Money& price,
        const std::string& reason
    ) {
        return Signal(id, strategyId, SignalType::HOLD, figi, price, reason);
    }

    /**
     * @brief Требует ли сигнал действия (не HOLD)
     */
    bool requiresAction() const {
        return type != SignalType::HOLD;
    }

    /**
     * @brief Это сигнал на покупку?
     */
    bool isBuy() const {
        return type == SignalType::BUY;
    }

    /**
     * @brief Это сигнал на продажу?
     */
    bool isSell() const {
        return type == SignalType::SELL;
    }
};

} // namespace trading::domain