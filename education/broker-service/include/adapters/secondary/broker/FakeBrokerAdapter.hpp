/**
 * @file FakeBrokerAdapter.hpp
 * @brief Адаптер для интеграции EnhancedFakeBroker с системой
 * 
 * Ключевые особенности:
 * 1. Автоматическая регистрация ТОЛЬКО sandbox-аккаунтов
 * 2. Персистентность через PostgreSQL репозитории
 * 3. Кэширование через ShardedCache (без TTL)
 * 4. БД - единственный источник правды
 * 5. EnhancedFakeBroker инжектится через DI
 * 6. Публикация portfolio.updated после исполнения ордеров
 */
#pragma once

#include "EnhancedFakeBroker.hpp"
#include "ports/output/IBrokerGateway.hpp"
#include "ports/output/IEventPublisher.hpp"
#include "ports/output/IBrokerBalanceRepository.hpp"
#include "ports/output/IBrokerPositionRepository.hpp"
#include "ports/output/IBrokerOrderRepository.hpp"
#include "ports/output/IQuoteRepository.hpp"
#include "ports/output/IInstrumentRepository.hpp"
#include "domain/events/OrderCreatedEvent.hpp"
#include "domain/events/OrderFilledEvent.hpp"
#include "domain/events/OrderCancelledEvent.hpp"
#include "domain/events/QuoteUpdatedEvent.hpp"

// Кэш из common-lib
#include <cache/concurrency/ShardedCache.hpp>
#include <cache/Cache.hpp>
#include <cache/eviction/LRUPolicy.hpp>

#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>
#include <string>
#include <chrono>
#include <stdexcept>


namespace broker::adapters::secondary {

// Константы
constexpr size_t CACHE_SHARD_COUNT = 16;
constexpr size_t CACHE_CAPACITY = 10000;
constexpr double SANDBOX_INITIAL_BALANCE = 100000.0;  // 100,000 RUB


/**
 * @brief Адаптер брокера с кэшированием и персистентностью
 */
class FakeBrokerAdapter : public ports::output::IBrokerGateway {
public:
    FakeBrokerAdapter(
        std::shared_ptr<EnhancedFakeBroker> broker,
        std::shared_ptr<ports::output::IEventPublisher> eventPublisher,
        std::shared_ptr<ports::output::IBrokerBalanceRepository> balanceRepo,
        std::shared_ptr<ports::output::IBrokerPositionRepository> positionRepo,
        std::shared_ptr<ports::output::IBrokerOrderRepository> orderRepo,
        std::shared_ptr<ports::output::IQuoteRepository> quoteRepo,
        std::shared_ptr<ports::output::IInstrumentRepository> instrumentRepo)
        : broker_(std::move(broker))
        , eventPublisher_(std::move(eventPublisher))
        , balanceRepo_(std::move(balanceRepo))
        , positionRepo_(std::move(positionRepo))
        , orderRepo_(std::move(orderRepo))
        , quoteRepo_(std::move(quoteRepo))
        , instrumentRepo_(std::move(instrumentRepo))
    {
        initCaches();
        loadFromDatabase();
        setupEventCallbacks();
        
        std::cout << "[FakeBrokerAdapter] Initialized with all repositories" << std::endl;
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
    // IBrokerGateway - ACCOUNT MANAGEMENT
    // ========================================================================
    
    void registerAccount(
        const std::string& accountId,
        const std::string& accessToken) override
    {
        if (!isSandboxAccount(accountId)) {
            throw std::runtime_error("Only sandbox accounts can be auto-registered. "
                                     "Account ID must contain 'sandbox'");
        }
        
        if (balanceCache_->contains(accountId)) {
            return;
        }
        
        domain::BrokerBalance balance;
        balance.accountId = accountId;
        balance.available = static_cast<int64_t>(SANDBOX_INITIAL_BALANCE * 100);
        balance.reserved = 0;
        balance.currency = "RUB";
        
        if (balanceRepo_) {
            try {
                balanceRepo_->save(balance);
                balanceCache_->put(accountId, balance);
                std::cout << "[FakeBrokerAdapter] Registered sandbox account: " << accountId << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[FakeBrokerAdapter] Failed to persist account: " << e.what() << std::endl;
                throw;
            }
        } else {
            balanceCache_->put(accountId, balance);
        }
        
        broker_->registerAccount(accountId, accessToken, SANDBOX_INITIAL_BALANCE);
    }
    
    void unregisterAccount(const std::string& accountId) override {
        broker_->unregisterAccount(accountId);
        balanceCache_->remove(accountId);
    }
    
    // ========================================================================
    // IBrokerGateway - MARKET DATA
    // ========================================================================
    
    std::optional<domain::Quote> getQuote(const std::string& figi) override {
        auto cached = quoteCache_->get(figi);
        if (cached) {
            return cached;
        }
        
        if (quoteRepo_) {
            auto fromDb = quoteRepo_->findByFigi(figi);
            if (fromDb) {
                quoteCache_->put(figi, *fromDb);
                return fromDb;
            }
        }
        
        auto brokerQuote = broker_->getQuote(figi);
        if (brokerQuote) {
            auto quote = convertQuote(*brokerQuote);
            quoteCache_->put(figi, quote);
            return quote;
        }
        
        return std::nullopt;
    }
    
    std::vector<domain::Quote> getQuotes(const std::vector<std::string>& figis) override {
        std::vector<domain::Quote> result;
        for (const auto& figi : figis) {
            if (auto q = getQuote(figi)) {
                result.push_back(*q);
            }
        }
        return result;
    }
    
    std::optional<domain::Instrument> getInstrumentByFigi(const std::string& figi) override {
        auto cached = instrumentCache_->get(figi);
        if (cached) {
            return cached;
        }
        
        if (instrumentRepo_) {
            auto fromDb = instrumentRepo_->findByFigi(figi);
            if (fromDb) {
                instrumentCache_->put(figi, *fromDb);
                return fromDb;
            }
        }
        
        auto brokerInstr = broker_->getInstrument(figi);
        if (brokerInstr) {
            auto instr = convertInstrument(*brokerInstr);
            instrumentCache_->put(figi, instr);
            return instr;
        }
        
        return std::nullopt;
    }
    
    std::vector<domain::Instrument> getAllInstruments() override {
        if (instrumentRepo_) {
            auto instruments = instrumentRepo_->findAll();
            for (const auto& instr : instruments) {
                instrumentCache_->put(instr.figi, instr);
            }
            return instruments;
        }
        
        std::vector<domain::Instrument> result;
        for (const auto& bi : broker_->getAllInstruments()) {
            result.push_back(convertInstrument(bi));
        }
        return result;
    }
    
    std::vector<domain::Instrument> searchInstruments(const std::string& query) override {
        auto brokerInstruments = broker_->searchInstruments(query);
        std::vector<domain::Instrument> result;
        result.reserve(brokerInstruments.size());
        
        for (const auto& bi : brokerInstruments) {
            result.push_back(convertInstrument(bi));
        }
        return result;
    }
    
    // ========================================================================
    // IBrokerGateway - PORTFOLIO
    // ========================================================================
    
    domain::Portfolio getPortfolio(const std::string& accountId) override {
        if (!accountExists(accountId)) {
            if (isSandboxAccount(accountId)) {
                registerAccount(accountId, "sandbox-token-" + accountId);
            } else {
                throw std::runtime_error("Account not found: " + accountId);
            }
        } else {
            ensureAccountInBroker(accountId);
        }
        
        domain::Portfolio portfolio;
        
        auto balance = getBalance(accountId);
        if (balance) {
            portfolio.cash = domain::Money::fromDouble(
                static_cast<double>(balance->available) / 100.0, 
                balance->currency
            );
        }
        
        portfolio.positions = getPositions(accountId);
        
        double totalValue = portfolio.cash.toDouble();
        for (const auto& pos : portfolio.positions) {
            totalValue += pos.currentPrice.toDouble() * pos.quantity;
        }
        portfolio.totalValue = domain::Money::fromDouble(totalValue, "RUB");
        
        return portfolio;
    }
    
    // ========================================================================
    // IBrokerGateway - ORDERS
    // ========================================================================
    
    domain::OrderResult placeOrder(
        const std::string& accountId,
        const domain::OrderRequest& request) override
    {
        if (!accountExists(accountId)) {
            if (isSandboxAccount(accountId)) {
                registerAccount(accountId, "sandbox-token-" + accountId);
            } else {
                domain::OrderResult rejected;
                rejected.status = domain::OrderStatus::REJECTED;
                rejected.message = "Account not found: " + accountId;
                return rejected;
            }
        } else {
            ensureAccountInBroker(accountId);
        }
        
        BrokerOrderRequest brokerReq;
        brokerReq.orderId = request.orderId;
        brokerReq.accountId = accountId;
        brokerReq.figi = request.figi;
        brokerReq.direction = convertDirection(request.direction);
        brokerReq.type = convertOrderType(request.type);
        brokerReq.quantity = request.quantity;
        brokerReq.price = request.price.toDouble();
        
        auto brokerResult = broker_->placeOrder(accountId, brokerReq);
        auto result = convertOrderResult(brokerResult);
        
        // orderId передан от trading-service (валидация в OrderCommandHandler)
        result.orderId = request.orderId;

        // Логируем результат
        if (result.status == domain::OrderStatus::REJECTED) {
            std::cout << "[FakeBrokerAdapter] REJECTED order=" << result.orderId 
                    << " reason=" << result.message << std::endl;
        } else {
            std::cout << "[FakeBrokerAdapter] Order " << result.orderId 
                    << " status=" << static_cast<int>(result.status) << std::endl;
        }

        // Сохраняем ордер ВСЕГДА (включая rejected)
        persistOrder(result.orderId, accountId, request, result);

        if (result.status == domain::OrderStatus::FILLED) {
            persistBalanceAndPositions(accountId);
            // Публикуем portfolio.updated после исполнения
            publishPortfolioUpdate(accountId);
        }
        
        return result;
    }
    
    bool cancelOrder(
        const std::string& accountId,
        const std::string& orderId) override
    {
        bool cancelled = broker_->cancelOrder(accountId, orderId);
        
        if (cancelled && orderRepo_) {
            auto order = orderRepo_->findById(orderId);
            if (order) {
                order->status = "CANCELLED";
                try {
                    orderRepo_->update(*order);
                    orderCache_->put(orderId, *order);
                } catch (...) {}
            }
        }
        
        return cancelled;
    }
    
    std::vector<domain::Order> getOrders(const std::string& accountId) override {
        if (!accountExists(accountId)) {
            if (isSandboxAccount(accountId)) {
                registerAccount(accountId, "sandbox-token-" + accountId);
                return {};
            } else {
                throw std::runtime_error("Account not found: " + accountId);
            }
        } else {
            ensureAccountInBroker(accountId);
        }
        
        std::vector<domain::Order> result;
        
        if (orderRepo_) {
            auto orders = orderRepo_->findByAccountId(accountId);
            for (const auto& bo : orders) {
                result.push_back(convertBrokerOrderToOrder(bo));
            }
        }
        
        return result;
    }
    
    std::optional<domain::Order> getOrderStatus(
        const std::string& accountId,
        const std::string& orderId) override
    {
        auto cached = orderCache_->get(orderId);
        if (cached) {
            return convertBrokerOrderToOrder(*cached);
        }
        
        if (orderRepo_) {
            auto fromDb = orderRepo_->findById(orderId);
            if (fromDb) {
                orderCache_->put(orderId, *fromDb);
                return convertBrokerOrderToOrder(*fromDb);
            }
        }
        
        return std::nullopt;
    }
    
    std::vector<domain::Order> getOrderHistory(
        const std::string& accountId,
        const std::optional<std::chrono::system_clock::time_point>& from = std::nullopt,
        const std::optional<std::chrono::system_clock::time_point>& to = std::nullopt) override
    {
        return getOrders(accountId);
    }


private:
    std::shared_ptr<EnhancedFakeBroker> broker_;
    std::shared_ptr<ports::output::IEventPublisher> eventPublisher_;
    std::shared_ptr<ports::output::IBrokerBalanceRepository> balanceRepo_;
    std::shared_ptr<ports::output::IBrokerPositionRepository> positionRepo_;
    std::shared_ptr<ports::output::IBrokerOrderRepository> orderRepo_;
    std::shared_ptr<ports::output::IQuoteRepository> quoteRepo_;
    std::shared_ptr<ports::output::IInstrumentRepository> instrumentRepo_;
    
    // Кэши
    std::unique_ptr<ShardedCache<std::string, domain::Quote, CACHE_SHARD_COUNT>> quoteCache_;
    std::unique_ptr<ShardedCache<std::string, domain::Instrument, CACHE_SHARD_COUNT>> instrumentCache_;
    std::unique_ptr<ShardedCache<std::string, domain::BrokerBalance, CACHE_SHARD_COUNT>> balanceCache_;
    std::unique_ptr<ShardedCache<std::string, domain::BrokerOrder, CACHE_SHARD_COUNT>> orderCache_;
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    void initCaches() {
        auto quoteCacheFactory = [](size_t capacity) {
            return std::make_unique<Cache<std::string, domain::Quote>>(
                capacity, std::make_unique<LRUPolicy<std::string>>());
        };
        quoteCache_ = std::make_unique<ShardedCache<std::string, domain::Quote, CACHE_SHARD_COUNT>>(
            CACHE_CAPACITY, quoteCacheFactory);
        
        auto instrumentCacheFactory = [](size_t capacity) {
            return std::make_unique<Cache<std::string, domain::Instrument>>(
                capacity, std::make_unique<LRUPolicy<std::string>>());
        };
        instrumentCache_ = std::make_unique<ShardedCache<std::string, domain::Instrument, CACHE_SHARD_COUNT>>(
            CACHE_CAPACITY, instrumentCacheFactory);
        
        auto balanceCacheFactory = [](size_t capacity) {
            return std::make_unique<Cache<std::string, domain::BrokerBalance>>(
                capacity, std::make_unique<LRUPolicy<std::string>>());
        };
        balanceCache_ = std::make_unique<ShardedCache<std::string, domain::BrokerBalance, CACHE_SHARD_COUNT>>(
            CACHE_CAPACITY, balanceCacheFactory);
        
        auto orderCacheFactory = [](size_t capacity) {
            return std::make_unique<Cache<std::string, domain::BrokerOrder>>(
                capacity, std::make_unique<LRUPolicy<std::string>>());
        };
        orderCache_ = std::make_unique<ShardedCache<std::string, domain::BrokerOrder, CACHE_SHARD_COUNT>>(
            CACHE_CAPACITY, orderCacheFactory);
    }
    
    void loadFromDatabase() {
        std::cout << "[FakeBrokerAdapter] Loading data from database..." << std::endl;
        
        if (instrumentRepo_) {
            auto instruments = instrumentRepo_->findAll();
            for (const auto& instr : instruments) {
                instrumentCache_->put(instr.figi, instr);
            }
            std::cout << "[FakeBrokerAdapter] Loaded " << instruments.size() << " instruments" << std::endl;
        }
        
        std::cout << "[FakeBrokerAdapter] Database load complete" << std::endl;
    }
    
    void ensureAccountInBroker(const std::string& accountId) {
        if (!broker_->hasAccount(accountId)) {
            auto balance = getBalance(accountId);
            double cash = balance ? static_cast<double>(balance->available) / 100.0 : SANDBOX_INITIAL_BALANCE;
            broker_->registerAccount(accountId, "restored-token", cash);
            
            if (positionRepo_) {
                auto positions = positionRepo_->findByAccountId(accountId);
                for (const auto& pos : positions) {
                    std::string ticker = pos.figi;
                    auto instr = getInstrumentByFigi(pos.figi);
                    if (instr) ticker = instr->ticker;
                    
                    broker_->importPosition(accountId, pos.figi, ticker, pos.quantity, pos.averagePrice);
                }
            }
        }
    }
    
    void setupEventCallbacks() {
        // Callback для обновления котировок
        broker_->setQuoteUpdateCallback([this](const BrokerQuoteUpdateEvent& e) {
            domain::Quote quote;
            quote.figi = e.figi;
            quote.lastPrice = domain::Money::fromDouble(e.last, "RUB");
            quote.bidPrice = domain::Money::fromDouble(e.bid, "RUB");
            quote.askPrice = domain::Money::fromDouble(e.ask, "RUB");
            
            if (quoteRepo_) {
                try {
                    quoteRepo_->save(quote);
                    quoteCache_->put(e.figi, quote);
                } catch (const std::exception& ex) {
                    std::cerr << "[FakeBrokerAdapter] Failed to persist quote: " << ex.what() << std::endl;
                }
            } else {
                quoteCache_->put(e.figi, quote);
            }
            
            if (eventPublisher_) {
                domain::QuoteUpdatedEvent event;
                event.figi = e.figi;
                event.lastPrice = quote.lastPrice;
                event.bidPrice = quote.bidPrice;
                event.askPrice = quote.askPrice;
                eventPublisher_->publish(event.eventType, event.toJson());
            }
        });
        
        // Callback для исполнения pending ордеров
        broker_->setOrderFillCallback([this](const BrokerOrderFillEvent& e) {
            // Обновляем портфель в БД
            persistBalanceAndPositions(e.accountId);
            
            // Публикуем portfolio.updated
            publishPortfolioUpdate(e.accountId);
            
            // Обновляем ордер
            if (orderRepo_) {
                auto order = orderRepo_->findById(e.orderId);
                if (order) {
                    order->executedLots += e.quantity;
                    order->status = (order->executedLots >= order->requestedLots) 
                        ? "FILLED" 
                        : "PARTIALLY_FILLED";
                    try {
                        orderRepo_->update(*order);
                        orderCache_->put(e.orderId, *order);
                    } catch (...) {}
                }
            }
            
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
    
    // ========================================================================
    // PORTFOLIO UPDATE PUBLISHER (НОВЫЙ МЕТОД)
    // ========================================================================
    
    /**
     * @brief Публикует событие portfolio.updated в RabbitMQ
     * 
     * Вызывается после исполнения ордера для уведомления trading-service
     * об изменении портфеля (позиций и баланса).
     */
    void publishPortfolioUpdate(const std::string& accountId) {
        if (!eventPublisher_) return;
        
        try {
            nlohmann::json event;
            event["account_id"] = accountId;
            event["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // Баланс
            auto balance = getBalance(accountId);
            if (balance) {
                event["cash"] = {
                    {"amount", static_cast<double>(balance->available) / 100.0},
                    {"currency", balance->currency}
                };
            }
            
            // Позиции
            event["positions"] = nlohmann::json::array();
            auto positions = getPositions(accountId);
            for (const auto& pos : positions) {
                nlohmann::json posJson;
                posJson["figi"] = pos.figi;
                posJson["ticker"] = pos.ticker;
                posJson["quantity"] = pos.quantity;
                posJson["average_price"] = pos.averagePrice.toDouble();
                posJson["current_price"] = pos.currentPrice.toDouble();
                posJson["currency"] = pos.averagePrice.currency;
                event["positions"].push_back(posJson);
            }
            
            // Общая стоимость
            double totalValue = balance ? static_cast<double>(balance->available) / 100.0 : 0.0;
            for (const auto& pos : positions) {
                totalValue += pos.currentPrice.toDouble() * pos.quantity;
            }
            event["total_value"] = {
                {"amount", totalValue},
                {"currency", "RUB"}
            };
            
            eventPublisher_->publish("portfolio.updated", event.dump());
            
            std::cout << "[FakeBrokerAdapter] Published portfolio.updated for " << accountId << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[FakeBrokerAdapter] Failed to publish portfolio.updated: " << e.what() << std::endl;
        }
    }
    
    // ========================================================================
    // HELPER METHODS
    // ========================================================================
    
    bool isSandboxAccount(const std::string& accountId) const {
        return accountId.find("sandbox") != std::string::npos;
    }
    
    bool accountExists(const std::string& accountId) const {
        if (balanceCache_->contains(accountId)) {
            return true;
        }
        
        if (balanceRepo_) {
            return balanceRepo_->findByAccountId(accountId).has_value();
        }
        
        return false;
    }
    
    std::optional<domain::BrokerBalance> getBalance(const std::string& accountId) {
        auto cached = balanceCache_->get(accountId);
        if (cached) {
            return cached;
        }
        
        if (balanceRepo_) {
            auto fromDb = balanceRepo_->findByAccountId(accountId);
            if (fromDb) {
                balanceCache_->put(accountId, *fromDb);
                return fromDb;
            }
        }
        
        return std::nullopt;
    }
    
    std::vector<domain::Position> getPositions(const std::string& accountId) {
        std::vector<domain::Position> result;
        
        if (positionRepo_) {
            auto positions = positionRepo_->findByAccountId(accountId);
            for (const auto& bp : positions) {
                domain::Position pos;
                pos.figi = bp.figi;
                
                auto instrument = getInstrumentByFigi(bp.figi);
                pos.ticker = instrument ? instrument->ticker : bp.figi;
                
                pos.quantity = bp.quantity;
                pos.averagePrice = domain::Money::fromDouble(bp.averagePrice, bp.currency);
                
                auto quote = getQuote(bp.figi);
                if (quote) {
                    pos.currentPrice = quote->lastPrice;
                } else {
                    pos.currentPrice = pos.averagePrice;
                }
                
                double avgPrice = bp.averagePrice;
                double curPrice = pos.currentPrice.toDouble();
                pos.pnl = domain::Money::fromDouble((curPrice - avgPrice) * bp.quantity, bp.currency);
                pos.pnlPercent = avgPrice > 0 ? ((curPrice - avgPrice) / avgPrice) * 100.0 : 0.0;
                
                result.push_back(pos);
            }
        }
        
        return result;
    }
    
    void persistOrder(const std::string& orderId, const std::string& accountId,
                      const domain::OrderRequest& request, const domain::OrderResult& result) {
        domain::BrokerOrder order;
        order.orderId = orderId;
        order.accountId = accountId;
        order.figi = request.figi;
        order.direction = (request.direction == domain::OrderDirection::BUY) ? "BUY" : "SELL";
        order.orderType = (request.type == domain::OrderType::MARKET) ? "MARKET" : "LIMIT";
        order.requestedLots = request.quantity;
        order.executedLots = result.executedLots;
        order.price = request.price.toDouble();
        order.executedPrice = result.executedPrice.toDouble();
        order.status = statusToString(result.status);
        
        if (orderRepo_) {
            try {
                orderRepo_->save(order);
                orderCache_->put(orderId, order);
            } catch (const std::exception& e) {
                std::cerr << "[FakeBrokerAdapter] Failed to persist order: " << e.what() << std::endl;
            }
        } else {
            orderCache_->put(orderId, order);
        }
    }
    
    std::string statusToString(domain::OrderStatus status) const {
        switch (status) {
            case domain::OrderStatus::PENDING: return "PENDING";
            case domain::OrderStatus::FILLED: return "FILLED";
            case domain::OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
            case domain::OrderStatus::CANCELLED: return "CANCELLED";
            case domain::OrderStatus::REJECTED: return "REJECTED";
            default: return "UNKNOWN";
        }
    }
    
    void persistBalanceAndPositions(const std::string& accountId) {
        auto portfolio = broker_->getPortfolio(accountId);
        
        domain::BrokerBalance balance;
        balance.accountId = accountId;
        balance.available = static_cast<int64_t>(portfolio.cash * 100);
        balance.reserved = 0;
        balance.currency = "RUB";
        
        if (balanceRepo_) {
            try {
                balanceRepo_->save(balance);
                balanceCache_->put(accountId, balance);
            } catch (...) {}
        } else {
            balanceCache_->put(accountId, balance);
        }
        
        if (positionRepo_) {
            for (const auto& pos : portfolio.positions) {
                domain::BrokerPosition dbPos;
                dbPos.accountId = accountId;
                dbPos.figi = pos.figi;
                dbPos.quantity = pos.quantity;
                dbPos.averagePrice = pos.averagePrice;
                dbPos.currency = "RUB";
                
                try {
                    positionRepo_->save(dbPos);
                } catch (...) {}
            }
        }
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
    
    domain::OrderResult convertOrderResult(const BrokerOrderResult& r) const {
        domain::OrderResult result;
        result.orderId = r.orderId;
        result.status = convertStatus(r.status);
        result.executedPrice = domain::Money::fromDouble(r.executedPrice, "RUB");
        result.executedLots = r.executedQuantity;
        result.message = r.message;
        return result;
    }
    
    domain::Order convertBrokerOrderToOrder(const domain::BrokerOrder& bo) const {
        domain::Order order;
        order.id = bo.orderId;
        order.accountId = bo.accountId;
        order.figi = bo.figi;
        order.direction = (bo.direction == "BUY") 
            ? domain::OrderDirection::BUY 
            : domain::OrderDirection::SELL;
        order.type = (bo.orderType == "MARKET") 
            ? domain::OrderType::MARKET 
            : domain::OrderType::LIMIT;
        order.quantity = bo.requestedLots;
        order.price = domain::Money::fromDouble(bo.price, "RUB");
        order.status = statusFromString(bo.status);
        return order;
    }
    
    domain::OrderStatus statusFromString(const std::string& s) const {
        if (s == "PENDING" || s == "NEW") return domain::OrderStatus::PENDING;
        if (s == "FILLED") return domain::OrderStatus::FILLED;
        if (s == "PARTIALLY_FILLED") return domain::OrderStatus::PARTIALLY_FILLED;
        if (s == "CANCELLED") return domain::OrderStatus::CANCELLED;
        if (s == "REJECTED") return domain::OrderStatus::REJECTED;
        return domain::OrderStatus::PENDING;
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
