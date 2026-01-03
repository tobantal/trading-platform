#pragma once

#include "domain/Instrument.hpp"
#include "domain/Quote.hpp"
#include "domain/Portfolio.hpp"
#include "domain/Order.hpp"
#include <string>
#include <vector>
#include <optional>

namespace trading::ports::output {

/**
 * @brief Интерфейс шлюза к Broker Service
 * 
 * Trading Service использует этот интерфейс для получения данных
 * от Broker Service через HTTP.
 * 
 * Примечание: Создание/отмена ордеров происходит через RabbitMQ,
 * здесь только read-only операции.
 */
class IBrokerGateway {
public:
    virtual ~IBrokerGateway() = default;

    // ============================================
    // РЫНОЧНЫЕ ДАННЫЕ
    // ============================================

    /**
     * @brief Получить котировку по FIGI
     */
    virtual std::optional<domain::Quote> getQuote(const std::string& figi) = 0;

    /**
     * @brief Получить котировки для списка инструментов
     */
    virtual std::vector<domain::Quote> getQuotes(const std::vector<std::string>& figis) = 0;

    // ============================================
    // ИНСТРУМЕНТЫ
    // ============================================

    /**
     * @brief Поиск инструментов
     */
    virtual std::vector<domain::Instrument> searchInstruments(const std::string& query) = 0;

    /**
     * @brief Получить инструмент по FIGI
     */
    virtual std::optional<domain::Instrument> getInstrumentByFigi(const std::string& figi) = 0;

    /**
     * @brief Получить все доступные инструменты
     */
    virtual std::vector<domain::Instrument> getAllInstruments() = 0;

    // ============================================
    // ДАННЫЕ АККАУНТА (read-only)
    // ============================================

    /**
     * @brief Получить портфель аккаунта
     */
    virtual domain::Portfolio getPortfolio(const std::string& accountId) = 0;

    /**
     * @brief Получить список ордеров аккаунта
     */
    virtual std::vector<domain::Order> getOrders(const std::string& accountId) = 0;

    /**
     * @brief Получить ордер по ID
     */
    virtual std::optional<domain::Order> getOrder(
        const std::string& accountId, 
        const std::string& orderId
    ) = 0;
};

} // namespace trading::ports::output
