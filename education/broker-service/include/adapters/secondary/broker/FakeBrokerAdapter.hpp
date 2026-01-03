/**
 * @file FakeBrokerAdapter.hpp
 * @brief Адаптер для интеграции EnhancedFakeBroker с системой
 * 
 * Ключевые особенности:
 * 1. Автоматическая регистрация ТОЛЬКО sandbox-аккаунтов
 *    (accountId содержит "sandbox")
 * 2. Персистентность через PostgreSQL репозитории
 * 3. Кэширование через ShardedCache (без TTL - данные живут вечно)
 * 4. БД - единственный источник правды, кэш восстанавливается из БД при старте
 * 5. Транзакционность: если запись в БД не удалась, кэш НЕ обновляется
 * 6. EnhancedFakeBroker инжектится через DI
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
 * 
 * ЕДИНСТВЕННЫЙ КОНСТРУКТОР - все зависимости обязательны и инжектятся через DI.
 */
class FakeBrokerAdapter : public ports::output::IBrokerGateway {
public:
    /**
     * @brief Конструктор
     * 
     * @param broker EnhancedFakeBroker (инжектится через DI)
     * @param eventPublisher Издатель событий (RabbitMQ)
     * @param balanceRepo Репозиторий балансов
     * @param positionRepo Репозиторий позиций
     * @param orderRepo Репозиторий ордеров
     * @param quoteRepo Репозиторий котировок
     * @param instrumentRepo Репозиторий инструментов
     */
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
        // Регистрируем только sandbox-аккаунты
        if (!isSandboxAccount(accountId)) {
            throw std::runtime_error("Only sandbox accounts can be auto-registered. "
                                     "Account ID must contain 'sandbox'");
        }
        
        // Проверяем, не существует ли уже
        if (balanceCache_->contains(accountId)) {
            return;  // Уже зарегистрирован
        }
        
        // Создаём баланс
        domain::BrokerBalance balance;
        balance.accountId = accountId;
        balance.available = static_cast<int64_t>(SANDBOX_INITIAL_BALANCE * 100);  // в копейках
        balance.reserved = 0;
        balance.currency = "RUB";
        
        // Сначала БД, потом кэш
        if (balanceRepo_) {
            try {
                balanceRepo_->save(balance);
                balanceCache_->put(accountId, balance);
                std::cout << "[FakeBrokerAdapter] Registered sandbox account: " << accountId << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[FakeBrokerAdapter] Failed to persist account: " << e.what() << std::endl;
                throw;  // Не регистрируем если БД недоступна
            }
        } else {
            // Без БД - только в память (для тестов)
            balanceCache_->put(accountId, balance);
        }
        
        // Регистрируем в симуляторе
        broker_->registerAccount(accountId, accessToken, SANDBOX_INITIAL_BALANCE);
    }
    
    void unregisterAccount(const std::string& accountId) override {
        broker_->unregisterAccount(accountId);
        balanceCache_->remove(accountId);
        // TODO: удалить из БД если нужно
    }
    
    // ========================================================================
    // IBrokerGateway - MARKET DATA
    // ========================================================================
    
    std::optional<domain::Quote> getQuote(const std::string& figi) override {
        // 1. Проверяем кэш
        auto cached = quoteCache_->get(figi);
        if (cached) {
            return cached;
        }
        
        // 2. БД
        if (quoteRepo_) {
            auto fromDb = quoteRepo_->findByFigi(figi);
            if (fromDb) {
                quoteCache_->put(figi, *fromDb);
                return fromDb;
            }
        }
        
        // 3. Симулятор
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
        // 1. Кэш
        auto cached = instrumentCache_->get(figi);
        if (cached) {
            return cached;
        }
        
        // 2. БД
        if (instrumentRepo_) {
            auto fromDb = instrumentRepo_->findByFigi(figi);
            if (fromDb) {
                instrumentCache_->put(figi, *fromDb);
                return fromDb;
            }
        }
        
        // 3. Симулятор
        auto brokerInstr = broker_->getInstrument(figi);
        if (brokerInstr) {
            auto instr = convertInstrument(*brokerInstr);
            instrumentCache_->put(figi, instr);
            return instr;
        }
        
        return std::nullopt;
    }
    
    std::vector<domain::Instrument> getAllInstruments() override {
        // Из БД (источник правды)
        if (instrumentRepo_) {
            auto instruments = instrumentRepo_->findAll();
            for (const auto& instr : instruments) {
                instrumentCache_->put(instr.figi, instr);
            }
            return instruments;
        }
        
        // Из симулятора
        std::vector<domain::Instrument> result;
        for (const auto& bi : broker_->getAllInstruments()) {
            result.push_back(convertInstrument(bi));
        }
        return result;
    }
    
    std::vector<domain::Instrument> searchInstruments(const std::string& query) override {
        // Используем поиск в симуляторе
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
        // Проверяем существование аккаунта
        if (!accountExists(accountId)) {
            // Пробуем авто-регистрацию sandbox
            if (isSandboxAccount(accountId)) {
                registerAccount(accountId, "sandbox-token-" + accountId);
            } else {
                throw std::runtime_error("Account not found: " + accountId);
            }
        }
        
        domain::Portfolio portfolio;
        portfolio.accountId = accountId;
        
        // Баланс из БД/кэша
        auto balance = getBalance(accountId);
        if (balance) {
            portfolio.cash = domain::Money::fromDouble(
                balance->available / 100.0, balance->currency);
        }
        
        // Позиции
        portfolio.positions = getPositions(accountId);
        
        // Total value
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
        // Проверяем/регистрируем аккаунт
        if (!accountExists(accountId)) {
            if (isSandboxAccount(accountId)) {
                registerAccount(accountId, "sandbox-token-" + accountId);
            } else {
                domain::OrderResult rejected;
                rejected.status = domain::OrderStatus::REJECTED;
                rejected.message = "Account not found: " + accountId;
                return rejected;
            }
        }
        
        // Конвертируем запрос
        BrokerOrderRequest brokerReq;
        brokerReq.accountId = accountId;
        brokerReq.figi = request.figi;
        brokerReq.direction = convertDirection(request.direction);
        brokerReq.type = convertOrderType(request.type);
        brokerReq.quantity = request.quantity;
        brokerReq.price = request.price.toDouble();
        
        // Размещаем через симулятор
        auto brokerResult = broker_->placeOrder(accountId, brokerReq);
        auto result = convertOrderResult(brokerResult);
        
        // Сохраняем в БД
        if (result.status != domain::OrderStatus::REJECTED) {
            persistOrder(result.orderId, accountId, request, result);
            
            // Публикуем событие
            if (eventPublisher_) {
                domain::OrderCreatedEvent event;
                event.orderId = result.orderId;
                event.accountId = accountId;
                event.figi = request.figi;
                event.direction = request.direction;
                event.quantity = request.quantity;
                event.price = request.price;
                eventPublisher_->publish(event.eventType, event.toJson());
            }
        }
        
        return result;
    }
    
    bool cancelOrder(
        const std::string& accountId,
        const std::string& orderId) override
    {
        bool cancelled = broker_->cancelOrder(accountId, orderId);
        
        // Обновляем в БД
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
        
        // Публикуем событие
        if (cancelled && eventPublisher_) {
            domain::OrderCancelledEvent event;
            event.orderId = orderId;
            event.accountId = accountId;
            eventPublisher_->publish(event.eventType, event.toJson());
        }
        
        return cancelled;
    }
    
    std::vector<domain::Order> getOrders(const std::string& accountId) override {
        // Проверяем существование аккаунта
        if (!accountExists(accountId)) {
            if (isSandboxAccount(accountId)) {
                registerAccount(accountId, "sandbox-token-" + accountId);
                return {};  // Новый аккаунт - пустые ордера
            } else {
                throw std::runtime_error("Account not found: " + accountId);
            }
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
        // 1. Кэш
        auto cached = orderCache_->get(orderId);
        if (cached) {
            return convertBrokerOrderToOrder(*cached);
        }
        
        // 2. БД
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
        // TODO: фильтрация по дате
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
    
    // Кэши (ShardedCache без TTL - данные живут вечно)
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
    
    /**
     * @brief Загрузить данные из БД в кэши при старте
     * БД - единственный источник правды
     */
    void loadFromDatabase() {
        std::cout << "[FakeBrokerAdapter] Loading data from database..." << std::endl;
        
        // Инструменты
        if (instrumentRepo_) {
            auto instruments = instrumentRepo_->findAll();
            for (const auto& instr : instruments) {
                instrumentCache_->put(instr.figi, instr);
            }
            std::cout << "[FakeBrokerAdapter] Loaded " << instruments.size() << " instruments" << std::endl;
        }
        
        std::cout << "[FakeBrokerAdapter] Database load complete" << std::endl;
    }
    
    void setupEventCallbacks() {
        // Callback для обновления котировок
        broker_->setQuoteUpdateCallback([this](const BrokerQuoteUpdateEvent& e) {
            domain::Quote quote;
            quote.figi = e.figi;
            quote.lastPrice = domain::Money::fromDouble(e.last, "RUB");
            quote.bidPrice = domain::Money::fromDouble(e.bid, "RUB");
            quote.askPrice = domain::Money::fromDouble(e.ask, "RUB");
            
            // Сначала БД, потом кэш
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
            
            // Публикуем событие
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
            // Обновляем портфель
            persistBalanceAndPositions(e.accountId);
            
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
    // HELPER METHODS
    // ========================================================================
    
    bool isSandboxAccount(const std::string& accountId) const {
        return accountId.find("sandbox") != std::string::npos;
    }
    
    bool accountExists(const std::string& accountId) const {
        // Кэш
        if (balanceCache_->contains(accountId)) {
            return true;
        }
        
        // БД
        if (balanceRepo_) {
            return balanceRepo_->findByAccountId(accountId).has_value();
        }
        
        return false;
    }
    
    std::optional<domain::BrokerBalance> getBalance(const std::string& accountId) {
        // 1. Кэш
        auto cached = balanceCache_->get(accountId);
        if (cached) {
            return cached;
        }
        
        // 2. БД
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
        
        // Баланс
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
        
        // Позиции
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
