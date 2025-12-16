#pragma once

#include "ports/output/IBrokerGateway.hpp"
#include <ThreadSafeMap.hpp>
#include <unordered_map>
#include <random>
#include <mutex>

namespace trading::adapters::secondary {

/**
 * @brief Эмуляция Tinkoff Invest API для MVP
 */
class FakeTinkoffAdapter : public ports::output::IBrokerGateway {
public:
    FakeTinkoffAdapter();
    ~FakeTinkoffAdapter() override = default;

    void setAccessToken(const std::string& token) override;

    std::optional<domain::Quote> getQuote(const std::string& figi) override;
    std::vector<domain::Quote> getQuotes(const std::vector<std::string>& figis) override;

    std::vector<domain::Instrument> searchInstruments(const std::string& query) override;
    std::optional<domain::Instrument> getInstrumentByFigi(const std::string& figi) override;
    std::vector<domain::Instrument> getAllInstruments() override;

    domain::Portfolio getPortfolio() override;
    std::vector<domain::Position> getPositions() override;
    domain::Money getCash() override;

    domain::OrderResult placeOrder(const domain::OrderRequest& request) override;
    bool cancelOrder(const std::string& orderId) override;
    std::optional<domain::Order> getOrderStatus(const std::string& orderId) override;
    std::vector<domain::Order> getOrders() override;

    // Для тестов: сброс состояния
    void reset();
    
    // Для тестов: установить cash напрямую
    void setCash(const domain::Money& cash);

private:
    domain::Money generatePrice(const std::string& figi);
    std::string generateUuid();
    void initInstruments();

    std::string accessToken_;
    std::unordered_map<std::string, domain::Instrument> instruments_;
    std::unordered_map<std::string, double> basePrices_;
    ThreadSafeMap<std::string, domain::Position> positions_;
    ThreadSafeMap<std::string, domain::Order> orders_;
    domain::Money cash_;
    
    mutable std::mt19937 rng_;
    mutable std::mutex rngMutex_;
};

} // namespace trading::adapters::secondary