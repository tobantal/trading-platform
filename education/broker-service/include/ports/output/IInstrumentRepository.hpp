// include/ports/output/IInstrumentRepository.hpp
#pragma once

#include "domain/Instrument.hpp"
#include <vector>
#include <optional>
#include <string>

namespace broker::ports::output {

/**
 * @brief Instrument repository interface (Output Port)
 */
class IInstrumentRepository {
public:
    virtual ~IInstrumentRepository() = default;

    virtual std::vector<domain::Instrument> findAll() = 0;
    virtual std::optional<domain::Instrument> findByFigi(const std::string& figi) = 0;
    virtual void save(const domain::Instrument& instrument) = 0;
};

} // namespace broker::ports::output
