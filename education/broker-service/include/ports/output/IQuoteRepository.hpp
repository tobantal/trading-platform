// include/ports/output/IQuoteRepository.hpp
#pragma once

#include "domain/Quote.hpp"
#include <optional>
#include <string>

namespace broker::ports::output {

/**
 * @brief Quote repository interface (Output Port)
 */
class IQuoteRepository {
public:
    virtual ~IQuoteRepository() = default;

    virtual std::optional<domain::Quote> findByFigi(const std::string& figi) = 0;
    virtual void save(const domain::Quote& quote) = 0;
};

} // namespace broker::ports::output
