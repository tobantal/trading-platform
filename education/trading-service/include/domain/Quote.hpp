#pragma once

#include "Money.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Котировка инструмента
 */
class Quote {
public:
    std::string figi;
    std::string ticker;
    Money lastPrice;
    Money bidPrice;
    Money askPrice;

    Quote() = default;

    Quote(const std::string& f, const std::string& t, 
          const Money& last, const Money& bid, const Money& ask)
        : figi(f), ticker(t), lastPrice(last), bidPrice(bid), askPrice(ask)
    {}

    double spread() const {
        return askPrice.toDouble() - bidPrice.toDouble();
    }

    double spreadPercent() const {
        if (lastPrice.toDouble() == 0) return 0;
        return (spread() / lastPrice.toDouble()) * 100.0;
    }
};

} // namespace trading::domain
