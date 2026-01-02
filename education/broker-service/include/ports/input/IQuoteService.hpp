// include/ports/input/IQuoteService.hpp
#pragma once

#include "domain/Instrument.hpp"
#include "domain/Quote.hpp"
#include <vector>
#include <optional>
#include <string>

namespace broker::ports::input {

/**
 * @brief Quote service interface (Input Port)
 */
class IQuoteService {
public:
    virtual ~IQuoteService() = default;

    /**
     * @brief Get all available instruments
     */
    virtual std::vector<domain::Instrument> getInstruments() = 0;

    /**
     * @brief Get instrument by FIGI
     */
    virtual std::optional<domain::Instrument> getInstrument(const std::string& figi) = 0;

    /**
     * @brief Get current quote for instrument
     */
    virtual std::optional<domain::Quote> getQuote(const std::string& figi) = 0;
};

} // namespace broker::ports::input
