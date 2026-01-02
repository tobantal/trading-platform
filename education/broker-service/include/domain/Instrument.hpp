#pragma once

#include "Money.hpp"
#include "Timestamp.hpp"
#include <string>

namespace broker::domain {

/**
 * @brief Ð˜Ð½Ñ„Ð¾Ñ€Ð¼Ð°Ñ†Ð¸Ñ Ð¾Ð± Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ðµ
 */
struct Instrument {
    std::string figi;           ///< FIGI
    std::string ticker;         ///< Ð¢Ð¸ÐºÐµÑ€
    std::string name;           ///< ÐŸÐ¾Ð»Ð½Ð¾Ðµ Ð½Ð°Ð·Ð²Ð°Ð½Ð¸Ðµ
    std::string currency;       ///< Ð’Ð°Ð»ÑŽÑ‚Ð° Ñ‚Ð¾Ñ€Ð³Ð¾Ð²
    int64_t lot = 1;            ///< Ð Ð°Ð·Ð¼ÐµÑ€ Ð»Ð¾Ñ‚Ð°
    Money minPriceIncrement;    ///< ÐœÐ¸Ð½Ð¸Ð¼Ð°Ð»ÑŒÐ½Ñ‹Ð¹ ÑˆÐ°Ð³ Ñ†ÐµÐ½Ñ‹
    bool tradingAvailable = true; ///< Ð”Ð¾ÑÑ‚ÑƒÐ¿ÐµÐ½ Ð´Ð»Ñ Ñ‚Ð¾Ñ€Ð³Ð¾Ð²

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
