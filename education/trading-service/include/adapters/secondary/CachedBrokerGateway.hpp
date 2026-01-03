#pragma once

#include "ports/output/IBrokerGateway.hpp"
#include "adapters/secondary/HttpBrokerGateway.hpp"
#include "settings/CacheSettings.hpp"
#include <cache/ICache.hpp>
#include <cache/Cache.hpp>
#include <cache/eviction/LRUPolicy.hpp>
#include <cache/expiration/GlobalTTL.hpp>
#include <cache/concurrency/ThreadSafeCache.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::secondary {

/**
 * @brief Декоратор IBrokerGateway с LRU кэшированием
 *
 * Два кэша:
 * - quoteCache_: figi -> Quote
 * - instrumentCache_: figi -> Instrument
 *
 * НЕ кэширует (всегда актуальные данные):
 * - Портфель
 * - Ордера
 */
class CachedBrokerGateway : public ports::output::IBrokerGateway {
public:
    CachedBrokerGateway(
        std::shared_ptr<HttpBrokerGateway> delegate,
        std::shared_ptr<settings::CacheSettings> cacheSettings
    ) : delegate_(std::move(delegate))
      , cacheSettings_(std::move(cacheSettings))
    {
        initCaches();
    }

    // ============================================
    // КОТИРОВКИ
    // ============================================

    std::optional<domain::Quote> getQuote(const std::string& figi) override {
        auto cached = quoteCache_->get(figi);
        if (cached) {
            return *cached;
        }

        auto quote = delegate_->getQuote(figi);
        if (quote) {
            quoteCache_->put(figi, *quote);
        }

        return quote;
    }

    std::vector<domain::Quote> getQuotes(const std::vector<std::string>& figis) override {
        std::vector<domain::Quote> result;
        std::vector<std::string> missingFigis;

        for (const auto& figi : figis) {
            auto cached = quoteCache_->get(figi);
            if (cached) {
                result.push_back(*cached);
            } else {
                missingFigis.push_back(figi);
            }
        }

        if (!missingFigis.empty()) {
            auto quotes = delegate_->getQuotes(missingFigis);
            for (const auto& quote : quotes) {
                quoteCache_->put(quote.figi, quote);
                result.push_back(quote);
            }
        }

        return result;
    }

    // ============================================
    // ИНСТРУМЕНТЫ
    // ============================================

    std::optional<domain::Instrument> getInstrumentByFigi(const std::string& figi) override {
        auto cached = instrumentCache_->get(figi);
        if (cached) {
            return *cached;
        }

        auto instrument = delegate_->getInstrumentByFigi(figi);
        if (instrument) {
            instrumentCache_->put(figi, *instrument);
        }

        return instrument;
    }

    std::vector<domain::Instrument> getAllInstruments() override {
        // ВСЕГДА делегируем - не знаем полная ли коллекция в кэше
        auto instruments = delegate_->getAllInstruments();

        // Прогреваем кэш
        for (const auto& instr : instruments) {
            instrumentCache_->put(instr.figi, instr);
        }

        return instruments;
    }

    std::vector<domain::Instrument> searchInstruments(const std::string& query) override {
        // TODO: broker-service не поддерживает search endpoint.
        // HttpBrokerGateway получает все инструменты и фильтрует.
        // ВСЕГДА делегируем, прогреваем кэш результатами.

        auto instruments = delegate_->searchInstruments(query);

        // Прогреваем кэш
        for (const auto& instr : instruments) {
            instrumentCache_->put(instr.figi, instr);
        }

        return instruments;
    }

    // ============================================
    // ДАННЫЕ АККАУНТА (БЕЗ кэширования)
    // ============================================

    domain::Portfolio getPortfolio(const std::string& accountId) override {
        return delegate_->getPortfolio(accountId);
    }

    std::vector<domain::Order> getOrders(const std::string& accountId) override {
        return delegate_->getOrders(accountId);
    }

    std::optional<domain::Order> getOrder(
        const std::string& accountId,
        const std::string& orderId
    ) override {
        return delegate_->getOrder(accountId, orderId);
    }

    // ============================================
    // УПРАВЛЕНИЕ КЭШЕМ
    // ============================================

    void clearQuoteCache() {
        quoteCache_->clear();
    }

    void clearInstrumentCache() {
        instrumentCache_->clear();
    }

    void clearAllCaches() {
        clearQuoteCache();
        clearInstrumentCache();
    }

    size_t getQuoteCacheSize() const {
        return quoteCache_->size();
    }

    size_t getInstrumentCacheSize() const {
        return instrumentCache_->size();
    }

private:
    void initCaches() {
        size_t quoteCacheSize = cacheSettings_->getQuoteCacheSize();
        int quoteTtlSeconds = cacheSettings_->getQuoteTtlSeconds();
        size_t instrumentCacheSize = cacheSettings_->getInstrumentCacheSize();
        int instrumentTtlSeconds = cacheSettings_->getInstrumentTtlSeconds();

        // Кэш котировок: figi -> Quote
        auto quoteBase = std::make_unique<Cache<std::string, domain::Quote>>(
            quoteCacheSize,
            std::make_unique<LRUPolicy<std::string>>(),
            std::make_unique<GlobalTTL<std::string>>(std::chrono::seconds(quoteTtlSeconds))
        );
        quoteCache_ = std::make_unique<ThreadSafeCache<std::string, domain::Quote>>(
            std::move(quoteBase)
        );

        // Кэш инструментов: figi -> Instrument
        auto instrumentBase = std::make_unique<Cache<std::string, domain::Instrument>>(
            instrumentCacheSize,
            std::make_unique<LRUPolicy<std::string>>(),
            std::make_unique<GlobalTTL<std::string>>(std::chrono::seconds(instrumentTtlSeconds))
        );
        instrumentCache_ = std::make_unique<ThreadSafeCache<std::string, domain::Instrument>>(
            std::move(instrumentBase)
        );

        std::cout << "[CachedBrokerGateway] Created with:"
                  << " quoteCache=" << quoteCacheSize << "/" << quoteTtlSeconds << "s"
                  << " instrumentCache=" << instrumentCacheSize << "/" << instrumentTtlSeconds << "s"
                  << std::endl;
    }

    std::shared_ptr<HttpBrokerGateway> delegate_;
    std::shared_ptr<settings::CacheSettings> cacheSettings_;
    std::unique_ptr<ICache<std::string, domain::Quote>> quoteCache_;
    std::unique_ptr<ICache<std::string, domain::Instrument>> instrumentCache_;
};

} // namespace trading::adapters::secondary

