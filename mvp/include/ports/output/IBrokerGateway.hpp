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
#include <memory>

namespace trading::ports::output {

/**
 * @brief Интерфейс шлюза к брокерскому API
 * 
 * Output Port для взаимодействия с Tinkoff Invest API.
 * 
 * Ключевые принципы:
 * 1. Поддержка множества аккаунтов (multi-account)
 * 2. Каждый аккаунт имеет свой контекст (токен, тип счета)
 * 3. Все методы работают в контексте конкретного аккаунта
 * 
 * Реализации:
 * - FakeTinkoffAdapter (MVP) - эмуляция без реального API
 * - TinkoffGrpcAdapter (Production) - реальный gRPC
 */
class IBrokerGateway {
public:
    virtual ~IBrokerGateway() = default;

    // ============================================
    // КОНФИГУРАЦИЯ АККАУНТОВ
    // ============================================

    /**
     * @brief Зарегистрировать аккаунт для работы
     * 
     * @param accountId ID аккаунта в нашей системе (должен соответствовать ID в Tinkoff)
     * @param accessToken Токен доступа для этого аккаунта
     */
    virtual void registerAccount(
        const std::string& accountId,
        const std::string& accessToken
    ) = 0;

    /**
     * @brief Удалить аккаунт из шлюза
     * 
     * @param accountId ID аккаунта
     */
    virtual void unregisterAccount(const std::string& accountId) = 0;

    // ============================================
    // РЫНОЧНЫЕ ДАННЫЕ
    // ============================================

    /**
     * @brief Получить котировку по FIGI
     * 
     * Использует дефолтный токен (первый зарегистрированный или специальный для маркетдаты)
     * 
     * @param figi FIGI инструмента
     * @return Quote или nullopt
     */
    virtual std::optional<domain::Quote> getQuote(const std::string& figi) = 0;

    /**
     * @brief Получить котировки для списка инструментов
     * 
     * Использует дефолтный токен
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
     * Использует дефолтный токен
     * 
     * @param query Поисковый запрос
     * @return Список найденных инструментов
     */
    virtual std::vector<domain::Instrument> searchInstruments(const std::string& query) = 0;

    /**
     * @brief Получить инструмент по FIGI
     * 
     * Использует дефолтный токен
     * 
     * @param figi FIGI инструмента
     * @return Instrument или nullopt
     */
    virtual std::optional<domain::Instrument> getInstrumentByFigi(const std::string& figi) = 0;

    /**
     * @brief Получить все доступные инструменты
     * 
     * Использует дефолтный токен
     * 
     * @return Список инструментов
     */
    virtual std::vector<domain::Instrument> getAllInstruments() = 0;

    // ============================================
    // ДАННЫЕ АККАУНТА (требуют accountId)
    // ============================================

    /**
     * @brief Получить портфель аккаунта
     * 
     * Использует токен, зарегистрированный для этого accountId
     * 
     * @param accountId ID аккаунта
     * @return Portfolio с позициями и кэшем
     */
    virtual domain::Portfolio getPortfolio(const std::string& accountId) = 0;

    // ============================================
    // ОРДЕРА (требуют accountId)
    // ============================================

    /**
     * @brief Разместить ордер
     * 
     * Использует токен, зарегистрированный для этого accountId
     * 
     * @param accountId ID аккаунта
     * @param request Параметры ордера (должен содержать figi, quantity, direction, type, price)
     * @return Результат размещения
     */
    virtual domain::OrderResult placeOrder(
        const std::string& accountId,
        const domain::OrderRequest& request
    ) = 0;

    /**
     * @brief Отменить ордер
     * 
     * Использует токен, зарегистрированный для этого accountId
     * 
     * @param accountId ID аккаунта
     * @param orderId ID ордера у брокера
     * @return true если отмена успешна
     */
    virtual bool cancelOrder(
        const std::string& accountId,
        const std::string& orderId
    ) = 0;

    /**
     * @brief Получить статус ордера
     * 
     * Использует токен, зарегистрированный для этого accountId
     * 
     * @param accountId ID аккаунта
     * @param orderId ID ордера у брокера
     * @return Order или nullopt
     */
    virtual std::optional<domain::Order> getOrderStatus(
        const std::string& accountId,
        const std::string& orderId
    ) = 0;

    /**
     * @brief Получить список активных ордеров аккаунта
     * 
     * Использует токен, зарегистрированный для этого accountId
     * 
     * @param accountId ID аккаунта
     * @return Список активных ордеров
     */
    virtual std::vector<domain::Order> getOrders(const std::string& accountId) = 0;

    /**
     * @brief Получить историю ордеров аккаунта
     * 
     * Использует токен, зарегистрированный для этого accountId
     * 
     * @param accountId ID аккаунта
     * @param from Начальная дата (опционально)
     * @param to Конечная дата (опционально)
     * @return Список ордеров за период
     */
    virtual std::vector<domain::Order> getOrderHistory(
        const std::string& accountId,
        const std::optional<std::chrono::system_clock::time_point>& from = std::nullopt,
        const std::optional<std::chrono::system_clock::time_point>& to = std::nullopt
    ) = 0;
};

} // namespace trading::ports::output