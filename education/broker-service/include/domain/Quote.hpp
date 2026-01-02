#pragma once

#include "Money.hpp"
#include "Timestamp.hpp"
#include <string>

namespace broker::domain {

/**
 * @brief Котировка финансового инструмента
 * 
 * Содержит текущие цены инструмента на бирже.
 */
struct Quote {
    std::string figi;           ///< FIGI инструмента (BBG004730N88)
    std::string ticker;         ///< Тикер (SBER)
    Money lastPrice;            ///< Цена последней сделки
    Money bidPrice;             ///< Лучшая цена покупки (bid)
    Money askPrice;             ///< Лучшая цена продажи (ask)
    Timestamp updatedAt;        ///< Время обновления

    Quote() = default;

    Quote(
        const std::string& figi,
        const std::string& ticker,
        const Money& lastPrice,
        const Money& bidPrice,
        const Money& askPrice
    ) : figi(figi), ticker(ticker), lastPrice(lastPrice),
        bidPrice(bidPrice), askPrice(askPrice), updatedAt(Timestamp::now()) {}

    /**
     * @brief Спред (разница между ask и bid)
     */
    Money spread() const {
        return askPrice - bidPrice;
    }

    /**
     * @brief Спред в процентах от цены
     */
    double spreadPercent() const {
        if (lastPrice.isZero()) return 0.0;
        return (spread().toDouble() / lastPrice.toDouble()) * 100.0;
    }
};

} // namespace broker::domain
