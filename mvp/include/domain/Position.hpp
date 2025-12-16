#pragma once

#include "Money.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Позиция в портфеле (владение инструментом)
 * 
 * Описывает текущее состояние позиции по конкретному инструменту:
 * количество, средняя цена покупки, текущая цена, P&L.
 */
struct Position {
    std::string figi;           ///< FIGI инструмента
    std::string ticker;         ///< Тикер для удобства
    int64_t quantity = 0;       ///< Количество в лотах
    Money averagePrice;         ///< Средняя цена покупки
    Money currentPrice;         ///< Текущая рыночная цена
    Money pnl;                  ///< Profit & Loss (абсолютный)
    double pnlPercent = 0.0;    ///< P&L в процентах

    Position() = default;

    Position(
        const std::string& figi,
        const std::string& ticker,
        int64_t quantity,
        const Money& averagePrice,
        const Money& currentPrice
    ) : figi(figi), ticker(ticker), quantity(quantity),
        averagePrice(averagePrice), currentPrice(currentPrice) {
        calculatePnl();
    }

    /**
     * @brief Пересчитать P&L
     */
    void calculatePnl() {
        if (quantity == 0 || averagePrice.isZero()) {
            pnl = Money(0, 0, averagePrice.currency);
            pnlPercent = 0.0;
            return;
        }

        // P&L = (currentPrice - averagePrice) * quantity
        Money diff = currentPrice - averagePrice;
        pnl = diff * quantity;
        
        // P&L % = (currentPrice - averagePrice) / averagePrice * 100
        pnlPercent = ((currentPrice.toDouble() - averagePrice.toDouble()) 
                      / averagePrice.toDouble()) * 100.0;
    }

    /**
     * @brief Общая стоимость позиции по текущей цене
     */
    Money marketValue() const {
        return currentPrice * quantity;
    }

    /**
     * @brief Общая стоимость позиции по средней цене покупки
     */
    Money costBasis() const {
        return averagePrice * quantity;
    }

    /**
     * @brief Обновить текущую цену и пересчитать P&L
     */
    void updateCurrentPrice(const Money& newPrice) {
        currentPrice = newPrice;
        calculatePnl();
    }
};

} // namespace trading::domain