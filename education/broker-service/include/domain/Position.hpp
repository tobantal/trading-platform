#pragma once

#include "Money.hpp"
#include <string>

namespace broker::domain {

/**
 * @brief ÐŸÐ¾Ð·Ð¸Ñ†Ð¸Ñ Ð² Ð¿Ð¾Ñ€Ñ‚Ñ„ÐµÐ»Ðµ (Ð²Ð»Ð°Ð´ÐµÐ½Ð¸Ðµ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð¾Ð¼)
 * 
 * ÐžÐ¿Ð¸ÑÑ‹Ð²Ð°ÐµÑ‚ Ñ‚ÐµÐºÑƒÑ‰ÐµÐµ ÑÐ¾ÑÑ‚Ð¾ÑÐ½Ð¸Ðµ Ð¿Ð¾Ð·Ð¸Ñ†Ð¸Ð¸ Ð¿Ð¾ ÐºÐ¾Ð½ÐºÑ€ÐµÑ‚Ð½Ð¾Ð¼Ñƒ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ñƒ:
 * ÐºÐ¾Ð»Ð¸Ñ‡ÐµÑÑ‚Ð²Ð¾, ÑÑ€ÐµÐ´Ð½ÑÑ Ñ†ÐµÐ½Ð° Ð¿Ð¾ÐºÑƒÐ¿ÐºÐ¸, Ñ‚ÐµÐºÑƒÑ‰Ð°Ñ Ñ†ÐµÐ½Ð°, P&L.
 */
struct Position {
    std::string figi;           ///< FIGI Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð°
    std::string ticker;         ///< Ð¢Ð¸ÐºÐµÑ€ Ð´Ð»Ñ ÑƒÐ´Ð¾Ð±ÑÑ‚Ð²Ð°
    int64_t quantity = 0;       ///< ÐšÐ¾Ð»Ð¸Ñ‡ÐµÑÑ‚Ð²Ð¾ Ð² Ð»Ð¾Ñ‚Ð°Ñ…
    Money averagePrice;         ///< Ð¡Ñ€ÐµÐ´Ð½ÑÑ Ñ†ÐµÐ½Ð° Ð¿Ð¾ÐºÑƒÐ¿ÐºÐ¸
    Money currentPrice;         ///< Ð¢ÐµÐºÑƒÑ‰Ð°Ñ Ñ€Ñ‹Ð½Ð¾Ñ‡Ð½Ð°Ñ Ñ†ÐµÐ½Ð°
    Money pnl;                  ///< Profit & Loss (Ð°Ð±ÑÐ¾Ð»ÑŽÑ‚Ð½Ñ‹Ð¹)
    double pnlPercent = 0.0;    ///< P&L Ð² Ð¿Ñ€Ð¾Ñ†ÐµÐ½Ñ‚Ð°Ñ…

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
     * @brief ÐŸÐµÑ€ÐµÑÑ‡Ð¸Ñ‚Ð°Ñ‚ÑŒ P&L
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
     * @brief ÐžÐ±Ñ‰Ð°Ñ ÑÑ‚Ð¾Ð¸Ð¼Ð¾ÑÑ‚ÑŒ Ð¿Ð¾Ð·Ð¸Ñ†Ð¸Ð¸ Ð¿Ð¾ Ñ‚ÐµÐºÑƒÑ‰ÐµÐ¹ Ñ†ÐµÐ½Ðµ
     */
    Money marketValue() const {
        return currentPrice * quantity;
    }

    /**
     * @brief ÐžÐ±Ñ‰Ð°Ñ ÑÑ‚Ð¾Ð¸Ð¼Ð¾ÑÑ‚ÑŒ Ð¿Ð¾Ð·Ð¸Ñ†Ð¸Ð¸ Ð¿Ð¾ ÑÑ€ÐµÐ´Ð½ÐµÐ¹ Ñ†ÐµÐ½Ðµ Ð¿Ð¾ÐºÑƒÐ¿ÐºÐ¸
     */
    Money costBasis() const {
        return averagePrice * quantity;
    }

    /**
     * @brief ÐžÐ±Ð½Ð¾Ð²Ð¸Ñ‚ÑŒ Ñ‚ÐµÐºÑƒÑ‰ÑƒÑŽ Ñ†ÐµÐ½Ñƒ Ð¸ Ð¿ÐµÑ€ÐµÑÑ‡Ð¸Ñ‚Ð°Ñ‚ÑŒ P&L
     */
    void updateCurrentPrice(const Money& newPrice) {
        currentPrice = newPrice;
        calculatePnl();
    }
};

} // namespace broker::domain
