#pragma once

#include "domain/Instrument.hpp"
#include "domain/Portfolio.hpp"
#include "domain/OrderRequest.hpp"
#include "domain/OrderResult.hpp"
#include "domain/Quote.hpp"
#include "domain/Position.hpp"
#include "domain/Order.hpp"
#include <string>
#include <vector>
#include <optional>

namespace trading::ports::output {

/**
 * @brief Интерфейс шлюза к брокерскому API
 * 
 * Output Port для взаимодействия с Tinkoff Invest API.
 * 
 * Реализации:
 * - FakeTinkoffAdapter (MVP) - эмуляция без реального API
 * - TinkoffGrpcAdapter (Production) - реальный gRPC
 */
class IBrokerGateway {
public:
    virtual ~IBrokerGateway() = default;

    // ============================================
    // КОНФИГУРАЦИЯ
    // ============================================

    /**
     * @brief Установить токен доступа для API запросов
     * 
     * @param token API токен Tinkoff
     */
    virtual void setAccessToken(const std::string& token) = 0;

    // ============================================
    // РЫНОЧНЫЕ ДАННЫЕ
    // ============================================

    /**
     * @brief Получить котировку по FIGI
     * 
     * @param figi FIGI инструмента
     * @return Quote или nullopt
     */
    virtual std::optional<domain::Quote> getQuote(const std::string& figi) = 0;

    /**
     * @brief Получить котировки для списка инструментов
     * 
     * @param figis Список FIGI
     * @return Список котировок
     */
    virtual std::vector<domain::Quote> getQuotes(const std::vector<std::string>& figis) = 0;

    // ============================================
    // ИНСТРУМЕНТЫ
    // ============================================

    /**
     * @brief Поиск инструментов
     * 
     * @param query Поисковый запрос
     * @return Список найденных инструментов
     */
    virtual std::vector<domain::Instrument> searchInstruments(const std::string& query) = 0;

    /**
     * @brief Получить инструмент по FIGI
     * 
     * @param figi FIGI инструмента
     * @return Instrument или nullopt
     */
    virtual std::optional<domain::Instrument> getInstrumentByFigi(const std::string& figi) = 0;

    /**
     * @brief Получить все доступные инструменты
     * 
     * @return Список инструментов
     */
    virtual std::vector<domain::Instrument> getAllInstruments() = 0;

    // ============================================
    // ПОРТФЕЛЬ
    // ============================================

    /**
     * @brief Получить портфель
     * 
     * @return Portfolio с позициями и кэшем
     */
    virtual domain::Portfolio getPortfolio() = 0;

    /**
     * @brief Получить список позиций
     * 
     * @return Список позиций
     */
    virtual std::vector<domain::Position> getPositions() = 0;

    /**
     * @brief Получить свободные денежные средства
     * 
     * @return Сумма кэша
     */
    virtual domain::Money getCash() = 0;

    // ============================================
    // ОРДЕРА
    // ============================================

    /**
     * @brief Разместить ордер
     * 
     * @param request Параметры ордера
     * @return Результат размещения
     */
    virtual domain::OrderResult placeOrder(const domain::OrderRequest& request) = 0;

    /**
     * @brief Отменить ордер
     * 
     * @param orderId ID ордера
     * @return true если отмена успешна
     */
    virtual bool cancelOrder(const std::string& orderId) = 0;

    /**
     * @brief Получить статус ордера
     * 
     * @param orderId ID ордера
     * @return Order или nullopt
     */
    virtual std::optional<domain::Order> getOrderStatus(const std::string& orderId) = 0;

    /**
     * @brief Получить список ордеров
     * 
     * @return Список активных ордеров
     */
    virtual std::vector<domain::Order> getOrders() = 0;
};

} // namespace trading::ports::output