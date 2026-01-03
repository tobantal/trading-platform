#pragma once

#include "domain/Quote.hpp"
#include "domain/Instrument.hpp"
#include <vector>
#include <string>
#include <optional>

namespace trading::ports::input {

/**
 * @brief Интерфейс сервиса рыночных данных
 */
class IMarketService {
public:
    virtual ~IMarketService() = default;

    /**
     * @brief Получить котировку по FIGI
     */
    virtual std::optional<domain::Quote> getQuote(const std::string& figi) = 0;

    /**
     * @brief Получить котировки для списка инструментов
     */
    virtual std::vector<domain::Quote> getQuotes(const std::vector<std::string>& figis) = 0;

    /**
     * @brief Поиск инструментов
     */
    virtual std::vector<domain::Instrument> searchInstruments(const std::string& query) = 0;

    /**
     * @brief Получить инструмент по FIGI
     */
    virtual std::optional<domain::Instrument> getInstrumentByFigi(const std::string& figi) = 0;

    /**
     * @brief Получить все инструменты
     */
    virtual std::vector<domain::Instrument> getAllInstruments() = 0;
};

} // namespace trading::ports::input
