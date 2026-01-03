#pragma once

#include <string>

namespace trading::domain {

/**
 * @brief Торговый инструмент
 */
class Instrument {
public:
    std::string figi;
    std::string ticker;
    std::string name;
    std::string currency;
    int lot;

    Instrument() : lot(1) {}

    Instrument(const std::string& f, const std::string& t,
               const std::string& n, const std::string& cur, int l)
        : figi(f), ticker(t), name(n), currency(cur), lot(l)
    {}
};

} // namespace trading::domain
