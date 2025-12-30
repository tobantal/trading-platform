#pragma once

#include "domain/Instrument.hpp"
#include "domain/Quote.hpp"
#include <string>
#include <vector>
#include <optional>

namespace trading::ports::input {

/**
 * @brief Интерфейс сервиса рыночных данных
 * 
 * Input Port для получения котировок и информации об инструментах.
 * 
 * @note Котировки не зависят от accountId - цена на рынке одна для всех.
 *       Токен для API устанавливается на уровне адаптера.
 */
class IMarketService {
public:
    virtual ~IMarketService() = default;

    /**
     * @brief Получить котировку по FIGI
     * 
     * @param figi FIGI инструмента (например, "BBG004730N88" для SBER)
     * @return Quote или nullopt если инструмент не найден
     */
    virtual std::optional<domain::Quote> getQuote(const std::string& figi) = 0;

    /**
     * @brief Получить котировки для списка инструментов
     * 
     * @param figis Список FIGI
     * @return Список котировок (только найденные)
     */
    virtual std::vector<domain::Quote> getQuotes(const std::vector<std::string>& figis) = 0;

    /**
     * @brief Поиск инструментов по тикеру или названию
     * 
     * @param query Поисковый запрос (тикер или часть названия)
     * @return Список найденных инструментов
     */
    virtual std::vector<domain::Instrument> searchInstruments(const std::string& query) = 0;

    /**
     * @brief Получить информацию об инструменте по FIGI
     * 
     * @param figi FIGI инструмента
     * @return Instrument или nullopt
     */
    virtual std::optional<domain::Instrument> getInstrumentByFigi(const std::string& figi) = 0;

    /**
     * @brief Получить список всех доступных инструментов
     * 
     * @return Список инструментов
     * 
     * @note В SimpleBrokerGatewayAdapter возвращает 5 захардкоженных инструментов
     */
    virtual std::vector<domain::Instrument> getAllInstruments() = 0;
};

} // namespace trading::ports::input