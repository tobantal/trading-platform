#pragma once

#include "ports/input/IMarketService.hpp"
#include "ports/output/IBrokerGateway.hpp"
#include <memory>
#include <iostream>

namespace trading::application {

/**
 * @brief Сервис рыночных данных
 * 
 * Получает данные через IBrokerGateway (который может быть
 * обёрнут в CachedBrokerGateway для кэширования).
 */
class MarketService : public ports::input::IMarketService {
public:
    explicit MarketService(
        std::shared_ptr<ports::output::IBrokerGateway> broker
    ) : broker_(std::move(broker))
    {
        std::cout << "[MarketService] Created" << std::endl;
    }

    /**
     * @brief Получить котировку по FIGI
     */
    std::optional<domain::Quote> getQuote(const std::string& figi) override {
        return broker_->getQuote(figi);
    }

    /**
     * @brief Получить котировки для списка инструментов
     */
    std::vector<domain::Quote> getQuotes(const std::vector<std::string>& figis) override {
        return broker_->getQuotes(figis);
    }

    /**
     * @brief Поиск инструментов по тикеру или названию
     */
    std::vector<domain::Instrument> searchInstruments(const std::string& query) override {
        return broker_->searchInstruments(query);
    }

    /**
     * @brief Получить информацию об инструменте по FIGI
     */
    std::optional<domain::Instrument> getInstrumentByFigi(const std::string& figi) override {
        return broker_->getInstrumentByFigi(figi);
    }

    /**
     * @brief Получить список всех доступных инструментов
     */
    std::vector<domain::Instrument> getAllInstruments() override {
        return broker_->getAllInstruments();
    }

private:
    std::shared_ptr<ports::output::IBrokerGateway> broker_;
};

} // namespace trading::application
