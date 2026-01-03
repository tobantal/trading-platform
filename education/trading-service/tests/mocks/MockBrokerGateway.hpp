#pragma once

#include "ports/output/IBrokerGateway.hpp"
#include <vector>
#include <map>
#include <optional>

namespace trading::tests {

/**
 * @brief Mock реализация IBrokerGateway для тестов
 */
class MockBrokerGateway : public ports::output::IBrokerGateway {
public:
    // Настройка ответов
    void setQuote(const std::string& figi, const domain::Quote& quote) {
        quotes_[figi] = quote;
    }

    void setInstrument(const std::string& figi, const domain::Instrument& instrument) {
        instruments_[figi] = instrument;
    }

    void setPortfolio(const std::string& accountId, const domain::Portfolio& portfolio) {
        portfolios_[accountId] = portfolio;
    }

    void setOrders(const std::string& accountId, const std::vector<domain::Order>& orders) {
        orders_[accountId] = orders;
    }

    // Счётчики вызовов
    int getQuoteCallCount() const { return getQuoteCallCount_; }
    int getQuotesCallCount() const { return getQuotesCallCount_; }
    int getInstrumentCallCount() const { return getInstrumentCallCount_; }
    int getAllInstrumentsCallCount() const { return getAllInstrumentsCallCount_; }
    int getPortfolioCallCount() const { return getPortfolioCallCount_; }
    int getOrdersCallCount() const { return getOrdersCallCount_; }

    void resetCallCounts() {
        getQuoteCallCount_ = 0;
        getQuotesCallCount_ = 0;
        getInstrumentCallCount_ = 0;
        getAllInstrumentsCallCount_ = 0;
        getPortfolioCallCount_ = 0;
        getOrdersCallCount_ = 0;
    }

    // IBrokerGateway implementation
    std::optional<domain::Quote> getQuote(const std::string& figi) override {
        ++getQuoteCallCount_;
        auto it = quotes_.find(figi);
        if (it != quotes_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<domain::Quote> getQuotes(const std::vector<std::string>& figis) override {
        ++getQuotesCallCount_;
        std::vector<domain::Quote> result;
        for (const auto& figi : figis) {
            auto it = quotes_.find(figi);
            if (it != quotes_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    std::vector<domain::Instrument> searchInstruments(const std::string& query) override {
        std::vector<domain::Instrument> result;
        for (const auto& [figi, instr] : instruments_) {
            if (instr.ticker.find(query) != std::string::npos ||
                instr.name.find(query) != std::string::npos) {
                result.push_back(instr);
            }
        }
        return result;
    }

    std::optional<domain::Instrument> getInstrumentByFigi(const std::string& figi) override {
        ++getInstrumentCallCount_;
        auto it = instruments_.find(figi);
        if (it != instruments_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<domain::Instrument> getAllInstruments() override {
        ++getAllInstrumentsCallCount_;
        std::vector<domain::Instrument> result;
        for (const auto& [figi, instr] : instruments_) {
            result.push_back(instr);
        }
        return result;
    }

    domain::Portfolio getPortfolio(const std::string& accountId) override {
        ++getPortfolioCallCount_;
        auto it = portfolios_.find(accountId);
        if (it != portfolios_.end()) {
            return it->second;
        }
        return domain::Portfolio();
    }

    std::vector<domain::Order> getOrders(const std::string& accountId) override {
        ++getOrdersCallCount_;
        auto it = orders_.find(accountId);
        if (it != orders_.end()) {
            return it->second;
        }
        return {};
    }

    std::optional<domain::Order> getOrder(
        const std::string& accountId, 
        const std::string& orderId
    ) override {
        auto it = orders_.find(accountId);
        if (it != orders_.end()) {
            for (const auto& order : it->second) {
                if (order.id == orderId) {
                    return order;
                }
            }
        }
        return std::nullopt;
    }

private:
    std::map<std::string, domain::Quote> quotes_;
    std::map<std::string, domain::Instrument> instruments_;
    std::map<std::string, domain::Portfolio> portfolios_;
    std::map<std::string, std::vector<domain::Order>> orders_;

    mutable int getQuoteCallCount_ = 0;
    mutable int getQuotesCallCount_ = 0;
    mutable int getInstrumentCallCount_ = 0;
    mutable int getAllInstrumentsCallCount_ = 0;
    mutable int getPortfolioCallCount_ = 0;
    mutable int getOrdersCallCount_ = 0;
};

} // namespace trading::tests
