#pragma once

#include "ports/output/IBrokerGateway.hpp"
#include <ThreadSafeMap.hpp>
#include <unordered_map>
#include <random>
#include <mutex>
#include <memory>

namespace trading::adapters::secondary {

class FakeTinkoffAdapter : public ports::output::IBrokerGateway {
public:
    FakeTinkoffAdapter();
    ~FakeTinkoffAdapter() override = default;

    // Конфигурация аккаунтов
    void registerAccount(const std::string& accountId, const std::string& accessToken) override;
    void unregisterAccount(const std::string& accountId) override;

    // Рыночные данные
    std::optional<domain::Quote> getQuote(const std::string& figi) override;
    std::vector<domain::Quote> getQuotes(const std::vector<std::string>& figis) override;

    // Инструменты
    std::vector<domain::Instrument> searchInstruments(const std::string& query) override;
    std::optional<domain::Instrument> getInstrumentByFigi(const std::string& figi) override;
    std::vector<domain::Instrument> getAllInstruments() override;

    // Данные аккаунта
    domain::Portfolio getPortfolio(const std::string& accountId) override;

    // Ордера
    domain::OrderResult placeOrder(const std::string& accountId, const domain::OrderRequest& request) override;
    bool cancelOrder(const std::string& accountId, const std::string& orderId) override;
    std::optional<domain::Order> getOrderStatus(const std::string& accountId, const std::string& orderId) override;
    std::vector<domain::Order> getOrders(const std::string& accountId) override;
    std::vector<domain::Order> getOrderHistory(
        const std::string& accountId,
        const std::optional<std::chrono::system_clock::time_point>& from = std::nullopt,
        const std::optional<std::chrono::system_clock::time_point>& to = std::nullopt) override;

    // Методы для тестов
    void reset();
    void setCash(const std::string& accountId, const domain::Money& cash);
    void setPositions(const std::string& accountId, const std::vector<domain::Position>& positions);

private:
    struct AccountData {
        std::string token;
        domain::Money cash;
        ThreadSafeMap<std::string, domain::Position> positions;
        ThreadSafeMap<std::string, domain::Order> orders;
        
        AccountData(const std::string& token) 
            : token(token)
            , cash(domain::Money::fromDouble(1000000.0, "RUB")) 
        {}
    };

    domain::Money generatePrice(const std::string& figi);
    std::string generateUuid();
    void initInstruments();
    
    std::shared_ptr<AccountData> getOrCreateAccount(const std::string& accountId);
    std::shared_ptr<AccountData> getAccount(const std::string& accountId);
    
    void updatePortfolioPrices(domain::Portfolio& portfolio);
    void executeOrder(AccountData& account, const domain::Order& order, const domain::Instrument& instrument);

    mutable std::mutex accountsMutex_;
    std::unordered_map<std::string, std::shared_ptr<AccountData>> accounts_;
    
    std::unordered_map<std::string, domain::Instrument> instruments_;
    std::unordered_map<std::string, double> basePrices_;
    
    mutable std::mt19937 rng_;
    mutable std::mutex rngMutex_;
};

} // namespace trading::adapters::secondary