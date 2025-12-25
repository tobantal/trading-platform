#pragma once

#include "ports/input/IMarketService.hpp"
#include "ports/output/IBrokerGateway.hpp"
#include "ports/output/ICachePort.hpp"
#include "ports/output/IEventBus.hpp"
#include "domain/events/QuoteUpdatedEvent.hpp"
#include <memory>
#include <nlohmann/json.hpp>

namespace trading::application {

/**
 * @brief Сервис рыночных данных
 * 
 * Реализует IMarketService, координирует работу между:
 * - IBrokerGateway (получение котировок от брокера)
 * - ICachePort (кэширование котировок)
 * - IEventBus (публикация событий обновления котировок)
 */
class MarketService : public ports::input::IMarketService {
public:
    MarketService(
        std::shared_ptr<ports::output::IBrokerGateway> broker,
        std::shared_ptr<ports::output::ICachePort> cache,
        std::shared_ptr<ports::output::IEventBus> eventBus
    ) : broker_(std::move(broker))
      , cache_(std::move(cache))
      , eventBus_(std::move(eventBus))
    {}

    /**
     * @brief Получить котировку по FIGI
     * 
     * Сначала проверяем кэш, если нет - запрашиваем у брокера.
     */
    std::optional<domain::Quote> getQuote(const std::string& figi) override {
        // Пробуем из кэша
        std::string cacheKey = "quote:" + figi;
        auto cached = cache_->get(cacheKey);
        if (cached) {
            return deserializeQuote(*cached);
        }

        // Запрашиваем у брокера
        auto quote = broker_->getQuote(figi);
        if (quote) {
            // Кэшируем на 10 секунд
            cache_->set(cacheKey, serializeQuote(*quote), 10);
            
            // Публикуем событие
            publishQuoteEvent(*quote);
        }

        return quote;
    }

    /**
     * @brief Получить котировки для списка инструментов
     */
    std::vector<domain::Quote> getQuotes(const std::vector<std::string>& figis) override {
        std::vector<domain::Quote> result;
        std::vector<std::string> missingFigis;

        // Сначала проверяем кэш
        for (const auto& figi : figis) {
            std::string cacheKey = "quote:" + figi;
            auto cached = cache_->get(cacheKey);
            if (cached) {
                if (auto quote = deserializeQuote(*cached)) {
                    result.push_back(*quote);
                    continue;
                }
            }
            missingFigis.push_back(figi);
        }

        // Запрашиваем отсутствующие у брокера
        if (!missingFigis.empty()) {
            auto brokerQuotes = broker_->getQuotes(missingFigis);
            for (const auto& quote : brokerQuotes) {
                std::string cacheKey = "quote:" + quote.figi;
                cache_->set(cacheKey, serializeQuote(quote), 10);
                result.push_back(quote);
                publishQuoteEvent(quote);
            }
        }

        return result;
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
        // Кэшируем информацию об инструментах дольше (они редко меняются)
        std::string cacheKey = "instrument:" + figi;
        auto cached = cache_->get(cacheKey);
        if (cached) {
            return deserializeInstrument(*cached);
        }

        auto instrument = broker_->getInstrumentByFigi(figi);
        if (instrument) {
            cache_->set(cacheKey, serializeInstrument(*instrument), 3600); // 1 час
        }

        return instrument;
    }

    /**
     * @brief Получить список всех доступных инструментов
     */
    std::vector<domain::Instrument> getAllInstruments() override {
        return broker_->getAllInstruments();
    }

private:
    std::shared_ptr<ports::output::IBrokerGateway> broker_;
    std::shared_ptr<ports::output::ICachePort> cache_;
    std::shared_ptr<ports::output::IEventBus> eventBus_;

    void publishQuoteEvent(const domain::Quote& quote) {
        domain::QuoteUpdatedEvent event;
        event.figi = quote.figi;
        event.ticker = quote.ticker;
        event.lastPrice = quote.lastPrice;
        event.bidPrice = quote.bidPrice;
        event.askPrice = quote.askPrice;
        eventBus_->publish(event);
    }

    std::string serializeQuote(const domain::Quote& quote) {
        nlohmann::json j;
        j["figi"] = quote.figi;
        j["ticker"] = quote.ticker;
        j["lastPrice"] = quote.lastPrice.toDouble();
        j["bidPrice"] = quote.bidPrice.toDouble();
        j["askPrice"] = quote.askPrice.toDouble();
        j["currency"] = quote.lastPrice.currency;
        return j.dump();
    }

    std::optional<domain::Quote> deserializeQuote(const std::string& json) {
        try {
            auto j = nlohmann::json::parse(json);
            std::string currency = j.value("currency", "RUB");
            return domain::Quote(
                j["figi"],
                j["ticker"],
                domain::Money::fromDouble(j["lastPrice"], currency),
                domain::Money::fromDouble(j["bidPrice"], currency),
                domain::Money::fromDouble(j["askPrice"], currency)
            );
        } catch (...) {
            return std::nullopt;
        }
    }

    std::string serializeInstrument(const domain::Instrument& instr) {
        nlohmann::json j;
        j["figi"] = instr.figi;
        j["ticker"] = instr.ticker;
        j["name"] = instr.name;
        j["currency"] = instr.currency;
        j["lot"] = instr.lot;
        return j.dump();
    }

    std::optional<domain::Instrument> deserializeInstrument(const std::string& json) {
        try {
            auto j = nlohmann::json::parse(json);
            return domain::Instrument(
                j["figi"],
                j["ticker"],
                j["name"],
                j["currency"],
                j["lot"]
            );
        } catch (...) {
            return std::nullopt;
        }
    }
};

} // namespace trading::application
