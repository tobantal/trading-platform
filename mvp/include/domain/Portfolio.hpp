#pragma once

#include "Money.hpp"
#include "Position.hpp"
#include <string>
#include <vector>

namespace trading::domain {

/**
 * @brief Портфель - совокупность позиций и денежных средств
 */
struct Portfolio {
    std::string accountId;              ///< ID счёта
    Money totalValue;                   ///< Общая стоимость портфеля
    Money cash;                         ///< Свободные денежные средства
    std::vector<Position> positions;    ///< Позиции по инструментам

    Portfolio() = default;

    /**
     * @brief Пересчитать общую стоимость портфеля
     */
    void recalculateTotalValue() {
        totalValue = cash;
        for (const auto& pos : positions) {
            totalValue = totalValue + pos.marketValue();
        }
    }

    /**
     * @brief Общий P&L по всем позициям
     */
    Money totalPnl() const {
        Money result(0, 0, cash.currency);
        for (const auto& pos : positions) {
            result = result + pos.pnl;
        }
        return result;
    }
};

} // namespace trading::domain