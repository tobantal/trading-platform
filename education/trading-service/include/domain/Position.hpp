#pragma once

#include "Money.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Позиция в портфеле
 */
class Position {
public:
    std::string figi;
    std::string ticker;
    int64_t quantity = 0;
    Money averagePrice;
    Money currentPrice;
    Money pnl;
    double pnlPercent = 0.0;

    Position() = default;

    void updatePnl() {
        if (quantity > 0) {
            double avgTotal = averagePrice.toDouble() * quantity;
            double curTotal = currentPrice.toDouble() * quantity;
            pnl = Money::fromDouble(curTotal - avgTotal, averagePrice.currency);
            
            if (avgTotal > 0) {
                pnlPercent = ((curTotal - avgTotal) / avgTotal) * 100.0;
            }
        }
    }
};

} // namespace trading::domain
