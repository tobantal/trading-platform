#pragma once


#include "ports/output/IBrokerGateway.hpp"
#include "EnhancedFakeBroker.hpp"
#include "ports/output/IEventPublisher.hpp"
#include "domain/events/OrderCreatedEvent.hpp"
#include "domain/events/OrderFilledEvent.hpp"
#include "domain/events/OrderCancelledEvent.hpp"
#include "domain/events/QuoteUpdatedEvent.hpp"
#include "domain/Money.hpp"


#include <memory>
#include <string>
#include <chrono>


namespace broker::adapters::secondary {


/**
 * @brief Адаптер для интеграции EnhancedFakeBroker с системой
 * 
 * Реализует IBrokerGateway и публикует события через IEventPublisher.
 */
class FakeBrokerAdapter : public ports::output::IBrokerGateway {
public:
    explicit FakeBrokerAdapter(
        std::shared_ptr<ports::output::IEventPublisher> eventPublisher,
        unsigned int seed = 0)
        : eventPublisher_(std::move(eventPublisher))
        , broker_(std::make_unique<EnhancedFakeBroker>(seed))
    {
        setupEventCallbacks();
        initDefaultAccounts();
    }
    
    ~FakeBrokerAdapter() override {
        broker_->stopSimulation();
    }
    
    // ========================================================================
    // SIMULATION CONTROL
    // ========================================================================
    
    void startSimulation(std::chrono::milliseconds interval = std::chrono::milliseconds{100}) {
        broker_->startSimulation(interval);
    }
    
    void stopSimulation() {
        broker_->stopSimulation();
    }
    
    bool isSimulationRunning() const {
        return broker_->isSimulationRunning();
    }
    
    // ========================================================================
    // SCENARIO CONFIGURATION (для тестов)
    // ========================================================================
    
    void setScenario(const std::string& figi, const MarketScenario& scenario) {
        broker_->setScenario(figi, scenario);
    }
    
    void setPrice(const std::string& figi, double price) {
        broker_->setPrice(figi, price);
    }
    
    void setCash(const std::string& accountId, const domain::Money& cash) {
        broker_->setCash(accountId, cash.toDouble());
    }
    
    EnhancedFakeBroker& broker() { return *broker_; }
    
    void reset() {
        broker_->reset();
        initDefaultAccounts();
    }
    
    // ========================================================================
    // IBrokerGateway - ACCOUNT MANAGEMENT
    // ========================================================================
    
    void registerAccount(
        const std::string& accountId,
        const std::string& accessToken) override
    {
        broker_->registerAccount(accountId, accessToken, 1000000.0);
    }
    
    void unregisterAccount(const std::string& accountId) override {
        broker_->unregisterAccount(accountId);
    }
    
    // ========================================================================
    // IBrokerGateway - MARKET DATA
    // ========================================================================
    
    std::optional<domain::Quote> getQuote(const std::string& figi) override {
        auto quote = broker_->getQuote(figi);
        if (!quote) {
            return std::nullopt;
        }
        return convertQuote(*quote);
    }
    
    std::vector<domain::Quote> getQuotes(const std::vector<std::string>& figis) override {
        std::vector<domain::Quote> result;
        auto quotes = broker_->getQuotes(figis);
        for (const auto& q : quotes) {
            result.push_back(convertQuote(q));
        }
        return result;
    }
    
    std::vector<domain::Instrument> searchInstruments(const std::string& query) override {
        std::vector<domain::Instrument> result;
        auto instruments = broker_->searchInstruments(query);
        for (const auto& i : instruments) {
            result.push_back(convertInstrument(i));
        }
        return result;
    }
    
    std::optional<domain::Instrument> getInstrumentByFigi(const std::string& figi) override {
        auto instr = broker_->getInstrument(figi);
        if (!instr) {
            return std::nullopt;
        }
        return convertInstrument(*instr);
    }
    
    std::vector<domain::Instrument> getAllInstruments() override {
        std::vector<domain::Instrument> result;
        auto instruments = broker_->getAllInstruments();
        for (const auto& i : instruments) {
            result.push_back(convertInstrument(i));
        }
        return result;
    }
    
    // ========================================================================
    // IBrokerGateway - PORTFOLIO
    // ========================================================================
    
    domain::Portfolio getPortfolio(const std::string& accountId) override {
        auto portfolio = broker_->getPortfolio(accountId);
        if (!portfolio) {
            throw std::runtime_error("Account not found: " + accountId);
        }
        return convertPortfolio(*portfolio);
    }
    
    // ========================================================================
    // IBrokerGateway - ORDERS
    // ========================================================================
    
    domain::OrderResult placeOrder(
        const std::string& accountId,
        const domain::OrderRequest& request) override
    {
        BrokerOrderRequest brokerReq;
        brokerReq.accountId = accountId;
        brokerReq.figi = request.figi;
        brokerReq.direction = convertDirection(request.direction);
        brokerReq.type = convertOrderType(request.type);
        brokerReq.quantity = request.quantity;
        brokerReq.price = request.price.toDouble();
        
        auto result = broker_->placeOrder(accountId, brokerReq);
        
        // Публикуем событие создания ордера
        if (eventPublisher_) {
            domain::OrderCreatedEvent event;
            event.orderId = result.orderId;
            event.accountId = accountId;
            event.figi = request.figi;
            event.quantity = request.quantity;
            event.direction = request.direction;
            event.orderType = request.type;
            eventPublisher_->publish(event.eventType, event.toJson());
            
            // Если сразу исполнен
            if (result.status == Status::FILLED || result.status == Status::PARTIALLY_FILLED) {
                domain::OrderFilledEvent fillEvent;
                fillEvent.orderId = result.orderId;
                fillEvent.accountId = accountId;
                fillEvent.figi = request.figi;
                fillEvent.quantity = result.executedQuantity;
                fillEvent.executedPrice = domain::Money::fromDouble(result.executedPrice, "RUB");
                eventPublisher_->publish(fillEvent.eventType, fillEvent.toJson());
            }
        }
        
        return convertOrderResult(result);
    }
    
    bool cancelOrder(
        const std::string& accountId,
        const std::string& orderId) override
    {
        bool cancelled = broker_->cancelOrder(accountId, orderId);
        
        if (cancelled && eventPublisher_) {
            domain::OrderCancelledEvent event;
            event.orderId = orderId;
            event.accountId = accountId;
            event.reason = "User requested";
            eventPublisher_->publish(event.eventType, event.toJson());
        }
        
        return cancelled;
    }
    
    std::optional<domain::Order> getOrderStatus(
        const std::string& accountId,
        const std::string& orderId) override
    {
        // TODO: EnhancedFakeBroker не хранит историю ордеров
        return std::nullopt;
    }
    
    std::vector<domain::Order> getOrders(const std::string& accountId) override
    {
        // TODO: EnhancedFakeBroker не хранит историю ордеров
        return {};
    }
    
    std::vector<domain::Order> getOrderHistory(
        const std::string& accountId,
        const std::optional<std::chrono::system_clock::time_point>& from = std::nullopt,
        const std::optional<std::chrono::system_clock::time_point>& to = std::nullopt) override
    {
        // TODO: EnhancedFakeBroker не хранит историю ордеров
        return {};
    }


private:
    std::shared_ptr<ports::output::IEventPublisher> eventPublisher_;
    std::unique_ptr<EnhancedFakeBroker> broker_;
    
    void setupEventCallbacks() {
        // Callback для обновления котировок
        broker_->setQuoteUpdateCallback([this](const BrokerQuoteUpdateEvent& e) {
            if (eventPublisher_) {
                domain::QuoteUpdatedEvent event;
                event.figi = e.figi;
                event.lastPrice = domain::Money::fromDouble(e.last, "RUB");
                event.bidPrice = domain::Money::fromDouble(e.bid, "RUB");
                event.askPrice = domain::Money::fromDouble(e.ask, "RUB");
                eventPublisher_->publish(event.eventType, event.toJson());
            }
        });
        
        // Callback для исполнения pending ордеров
        broker_->setOrderFillCallback([this](const BrokerOrderFillEvent& e) {
            if (eventPublisher_) {
                domain::OrderFilledEvent event;
                event.orderId = e.orderId;
                event.accountId = e.accountId;
                event.figi = e.figi;
                event.quantity = e.quantity;
                event.executedPrice = domain::Money::fromDouble(e.price, "RUB");
                eventPublisher_->publish(event.eventType, event.toJson());
            }
        });
    }
    
    void initDefaultAccounts() {
        broker_->registerAccount("acc-001-sandbox", "token-1", 1000000.0);
        broker_->registerAccount("acc-001-prod", "token-2", 500000.0);
        broker_->registerAccount("acc-002-sandbox", "token-3", 100000.0);
        broker_->registerAccount("acc-004-sandbox", "token-4", 10000000.0);
    }
    
    // ========================================================================
    // TYPE CONVERSIONS
    // ========================================================================
    
    domain::Quote convertQuote(const BrokerQuote& q) const {
        domain::Quote quote;
        quote.figi = q.figi;
        quote.ticker = q.ticker;
        quote.lastPrice = domain::Money::fromDouble(q.lastPrice, "RUB");
        quote.bidPrice = domain::Money::fromDouble(q.bidPrice, "RUB");
        quote.askPrice = domain::Money::fromDouble(q.askPrice, "RUB");
        return quote;
    }
    
    domain::Instrument convertInstrument(const BrokerInstrument& i) const {
        domain::Instrument instr;
        instr.figi = i.figi;
        instr.ticker = i.ticker;
        instr.name = i.name;
        instr.currency = i.currency;
        instr.lot = i.lot;
        instr.minPriceIncrement = domain::Money::fromDouble(i.minPriceIncrement, i.currency);
        return instr;
    }
    
    domain::Portfolio convertPortfolio(const BrokerPortfolio& p) const {
        domain::Portfolio portfolio;
        portfolio.accountId = p.accountId;
        portfolio.cash = domain::Money::fromDouble(p.cash, p.currency);
        
        for (const auto& pos : p.positions) {
            domain::Position domainPos;
            domainPos.figi = pos.figi;
            domainPos.ticker = pos.ticker;
            domainPos.quantity = pos.quantity;
            domainPos.averagePrice = domain::Money::fromDouble(pos.averagePrice, "RUB");
            domainPos.currentPrice = domain::Money::fromDouble(pos.currentPrice, "RUB");
            portfolio.positions.push_back(domainPos);
        }
        
        return portfolio;
    }
    
    domain::OrderResult convertOrderResult(const BrokerOrderResult& r) const {
        domain::OrderResult result;
        result.orderId = r.orderId;
        result.status = convertStatus(r.status);
        result.executedPrice = domain::Money::fromDouble(r.executedPrice, "RUB");
        result.executedLots = r.executedQuantity;
        result.message = r.message;
        return result;
    }
    
    Direction convertDirection(domain::OrderDirection d) const {
        return (d == domain::OrderDirection::BUY) ? Direction::BUY : Direction::SELL;
    }
    
    Type convertOrderType(domain::OrderType t) const {
        return (t == domain::OrderType::MARKET) ? Type::MARKET : Type::LIMIT;
    }
    
    domain::OrderStatus convertStatus(Status s) const {
        switch (s) {
            case Status::PENDING: return domain::OrderStatus::PENDING;
            case Status::FILLED: return domain::OrderStatus::FILLED;
            case Status::PARTIALLY_FILLED: return domain::OrderStatus::PARTIALLY_FILLED;
            case Status::CANCELLED: return domain::OrderStatus::CANCELLED;
            case Status::REJECTED: return domain::OrderStatus::REJECTED;
            default: return domain::OrderStatus::REJECTED;
        }
    }
};


} // namespace broker::adapters::secondary