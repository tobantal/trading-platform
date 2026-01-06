#pragma once

#include "Money.hpp"
#include "Timestamp.hpp"
#include <string>

namespace broker::domain {

/**
 * @brief Информация об инструменте
 */
struct Instrument {
    std::string figi;           ///< FIGI
    std::string ticker;         ///< Тикер
    std::string name;           ///< Полное название
    std::string currency;       ///< Валюта торгов
    int64_t lot = 1;            ///< Размер лота
    Money minPriceIncrement;    ///< Минимальный шаг цены
    bool tradingAvailable = true; ///< Доступен для торгов

    Instrument() = default;

    Instrument(
        const std::string& figi,
        const std::string& ticker,
        const std::string& name,
        const std::string& currency,
        int64_t lot
    ) : figi(figi), ticker(ticker), name(name), currency(currency), lot(lot) {}
};

} // namespace broker::domain