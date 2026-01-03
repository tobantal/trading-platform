#pragma once

#include "Money.hpp"
#include "Position.hpp"
#include <vector>

namespace trading::domain {

/**
 * @brief Портфель пользователя
 */
class Portfolio {
public:
    Money cash;
    Money totalValue;
    std::vector<Position> positions;

    Portfolio() = default;

    Money totalPnl() const {
        Money total = Money::fromDouble(0.0, cash.currency);
        for (const auto& pos : positions) {
            total = total + pos.pnl;
        }
        return total;
    }

    double totalPnlPercent() const {
        double costBasis = totalValue.toDouble() - totalPnl().toDouble();
        if (costBasis <= 0) return 0.0;
        return (totalPnl().toDouble() / costBasis) * 100.0;
    }
};

} // namespace trading::domain
