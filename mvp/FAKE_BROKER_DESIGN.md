# FakeBroker Design Document

## Проблема

Текущий `FakeTinkoffAdapter` имеет ограничения:
- Все market-ордера мгновенно исполняются по фиксированной цене
- Limit-ордера уходят в PENDING навсегда
- Нет симуляции книги заявок, спреда, ликвидности
- Невозможно тестировать realistic сценарии

## Предлагаемая архитектура

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         FakeBrokerAdapter                               │
├─────────────────────────────────────────────────────────────────────────┤
│  ┌──────────────────┐  ┌──────────────────┐  ┌────────────────────────┐ │
│  │  PriceSimulator  │  │  OrderProcessor  │  │   MarketScenario       │ │
│  │                  │  │                  │  │   Configuration        │ │
│  │  - Генерация цен │  │  - Обработка     │  │                        │ │
│  │  - Random walk   │  │    ордеров       │  │  - Fill behavior       │ │
│  │  - Спред bid/ask │  │  - Partial fills │  │  - Liquidity           │ │
│  │  - Волатильность │  │  - Slippage      │  │  - Delays              │ │
│  └────────┬─────────┘  └────────┬─────────┘  └────────────────────────┘ │
│           │                     │                                       │
│           └──────────┬──────────┘                                       │
│                      │                                                  │
│           ┌──────────▼──────────┐                                       │
│           │     EventBus        │                                       │
│           │  (публикация        │                                       │
│           │   событий)          │                                       │
│           └─────────────────────┘                                       │
└─────────────────────────────────────────────────────────────────────────┘
```

## Компоненты

### 1. MarketScenario — Конфигурация сценария

```cpp
namespace trading::adapters::secondary {

/**
 * @brief Поведение исполнения ордеров
 */
enum class OrderFillBehavior {
    IMMEDIATE,        // Мгновенное исполнение (текущее поведение)
    REALISTIC,        // Реалистичное: market=fill, limit=pending до цены
    PARTIAL,          // Частичное исполнение
    DELAYED,          // С задержкой (async)
    ALWAYS_REJECT     // Всегда отклонять (тест ошибок)
};

/**
 * @brief Конфигурация рыночного сценария для тестирования
 */
struct MarketScenario {
    // --- Ценовые параметры ---
    double basePrice = 100.0;           // Базовая цена
    double bidAskSpread = 0.1;          // Спред (в %)
    double volatility = 0.02;           // Волатильность (в %)
    
    // --- Ликвидность ---
    int64_t availableLiquidity = 10000; // Доступный объём
    double slippagePercent = 0.01;      // Проскальзывание при большом объёме
    
    // --- Поведение ордеров ---
    OrderFillBehavior fillBehavior = OrderFillBehavior::REALISTIC;
    std::chrono::milliseconds fillDelay{0};  // Задержка исполнения
    double partialFillRatio = 1.0;           // Доля исполнения (0.0-1.0)
    
    // --- Rejection ---
    double rejectProbability = 0.0;     // Вероятность отклонения
    std::string rejectReason = "";      // Причина отклонения
};

} // namespace
```

### 2. PriceSimulator — Генератор цен

```cpp
namespace trading::adapters::secondary {

/**
 * @brief Симулятор рыночных цен
 * 
 * Генерирует реалистичные bid/ask цены с:
 * - Random walk (случайное блуждание)
 * - Настраиваемой волатильностью
 * - Bid-ask спредом
 */
class PriceSimulator {
public:
    struct Quote {
        double bid;
        double ask;
        double last;
        int64_t volume;
        domain::Timestamp timestamp;
    };
    
    /**
     * @brief Инициализировать инструмент
     */
    void initInstrument(const std::string& figi, double basePrice, double spread, double volatility) {
        InstrumentState state;
        state.currentPrice = basePrice;
        state.spread = spread;
        state.volatility = volatility;
        instruments_[figi] = state;
    }
    
    /**
     * @brief Получить текущую котировку
     */
    Quote getQuote(const std::string& figi) {
        auto& state = instruments_[figi];
        
        Quote q;
        q.last = state.currentPrice;
        q.bid = state.currentPrice * (1.0 - state.spread / 2.0);
        q.ask = state.currentPrice * (1.0 + state.spread / 2.0);
        q.volume = state.dailyVolume;
        q.timestamp = domain::Timestamp::now();
        
        return q;
    }
    
    /**
     * @brief Симулировать тик (движение цены)
     * 
     * Использует geometric Brownian motion:
     * dS = μ*S*dt + σ*S*dW
     */
    void tick(const std::string& figi) {
        auto& state = instruments_[figi];
        
        // Random walk
        std::normal_distribution<double> dist(0.0, state.volatility);
        double change = dist(rng_);
        
        state.currentPrice *= (1.0 + change);
        state.currentPrice = std::max(0.01, state.currentPrice); // Не уходим в минус
    }
    
    /**
     * @brief Симулировать N тиков
     */
    void simulate(const std::string& figi, int ticks) {
        for (int i = 0; i < ticks; ++i) {
            tick(figi);
        }
    }
    
    /**
     * @brief Установить конкретную цену (для детерминированных тестов)
     */
    void setPrice(const std::string& figi, double price) {
        instruments_[figi].currentPrice = price;
    }
    
    /**
     * @brief Сдвинуть цену на delta
     */
    void movePrice(const std::string& figi, double delta) {
        instruments_[figi].currentPrice += delta;
    }

private:
    struct InstrumentState {
        double currentPrice = 100.0;
        double spread = 0.001;      // 0.1%
        double volatility = 0.001;  // 0.1% per tick
        int64_t dailyVolume = 1000000;
    };
    
    std::unordered_map<std::string, InstrumentState> instruments_;
    std::mt19937 rng_{std::random_device{}()};
};

} // namespace
```

### 3. OrderProcessor — Обработчик ордеров

```cpp
namespace trading::adapters::secondary {

/**
 * @brief Процессор ордеров с реалистичной симуляцией
 */
class OrderProcessor {
public:
    OrderProcessor(
        std::shared_ptr<PriceSimulator> priceSimulator,
        std::shared_ptr<ports::output::IEventBus> eventBus
    ) : priceSimulator_(priceSimulator)
      , eventBus_(eventBus)
    {}
    
    /**
     * @brief Обработать ордер согласно сценарию
     */
    domain::OrderResult processOrder(
        const domain::OrderRequest& request,
        const MarketScenario& scenario)
    {
        // 1. Проверка rejection
        if (shouldReject(scenario)) {
            return rejectOrder(request, scenario.rejectReason);
        }
        
        // 2. Получаем текущую цену
        auto quote = priceSimulator_->getQuote(request.figi);
        
        // 3. Обрабатываем в зависимости от типа
        switch (scenario.fillBehavior) {
            case OrderFillBehavior::IMMEDIATE:
                return fillImmediately(request, quote);
                
            case OrderFillBehavior::REALISTIC:
                return processRealistic(request, quote, scenario);
                
            case OrderFillBehavior::PARTIAL:
                return fillPartially(request, quote, scenario);
                
            case OrderFillBehavior::DELAYED:
                return queueForDelayedFill(request, quote, scenario);
                
            case OrderFillBehavior::ALWAYS_REJECT:
                return rejectOrder(request, "Test rejection");
        }
        
        return rejectOrder(request, "Unknown fill behavior");
    }
    
    /**
     * @brief Обработать pending ордера (вызывается периодически)
     * 
     * Проверяет limit-ордера и исполняет если цена достигла лимита
     */
    void processPendingOrders() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto it = pendingOrders_.begin(); it != pendingOrders_.end(); ) {
            auto& order = it->second;
            auto quote = priceSimulator_->getQuote(order.figi);
            
            bool shouldFill = false;
            double fillPrice = 0.0;
            
            if (order.type == domain::OrderType::LIMIT) {
                if (order.direction == domain::OrderDirection::BUY) {
                    // Buy limit: исполняется когда ask <= limit price
                    shouldFill = quote.ask <= order.price.toDouble();
                    fillPrice = std::min(quote.ask, order.price.toDouble());
                } else {
                    // Sell limit: исполняется когда bid >= limit price
                    shouldFill = quote.bid >= order.price.toDouble();
                    fillPrice = std::max(quote.bid, order.price.toDouble());
                }
            }
            
            if (shouldFill) {
                // Исполняем
                publishFillEvent(order, fillPrice);
                it = pendingOrders_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    /**
     * @brief Принудительно исполнить ордер (для тестов)
     */
    void forceFillOrder(const std::string& orderId, double price) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = pendingOrders_.find(orderId);
        if (it != pendingOrders_.end()) {
            publishFillEvent(it->second, price);
            pendingOrders_.erase(it);
        }
    }
    
    /**
     * @brief Принудительно отклонить ордер (для тестов)
     */
    void forceRejectOrder(const std::string& orderId, const std::string& reason) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = pendingOrders_.find(orderId);
        if (it != pendingOrders_.end()) {
            publishRejectEvent(it->second, reason);
            pendingOrders_.erase(it);
        }
    }

private:
    std::shared_ptr<PriceSimulator> priceSimulator_;
    std::shared_ptr<ports::output::IEventBus> eventBus_;
    
    std::mutex mutex_;
    std::unordered_map<std::string, domain::Order> pendingOrders_;
    
    bool shouldReject(const MarketScenario& scenario) {
        if (scenario.rejectProbability <= 0.0) return false;
        
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        std::mt19937 rng{std::random_device{}()};
        return dist(rng) < scenario.rejectProbability;
    }
    
    domain::OrderResult fillImmediately(
        const domain::OrderRequest& request,
        const PriceSimulator::Quote& quote)
    {
        double fillPrice = (request.direction == domain::OrderDirection::BUY)
            ? quote.ask
            : quote.bid;
        
        return domain::OrderResult{
            .orderId = generateOrderId(),
            .status = domain::OrderStatus::FILLED,
            .executedPrice = domain::Money::fromDouble(fillPrice, "RUB"),
            .executedQuantity = request.quantity,
            .message = "Filled immediately",
            .timestamp = domain::Timestamp::now()
        };
    }
    
    domain::OrderResult processRealistic(
        const domain::OrderRequest& request,
        const PriceSimulator::Quote& quote,
        const MarketScenario& scenario)
    {
        if (request.type == domain::OrderType::MARKET) {
            // Market order: fill сразу с возможным slippage
            double basePrice = (request.direction == domain::OrderDirection::BUY)
                ? quote.ask
                : quote.bid;
            
            // Slippage при большом объёме
            double slippage = 0.0;
            if (request.quantity > scenario.availableLiquidity * 0.1) {
                slippage = basePrice * scenario.slippagePercent * 
                    (static_cast<double>(request.quantity) / scenario.availableLiquidity);
            }
            
            double fillPrice = (request.direction == domain::OrderDirection::BUY)
                ? basePrice + slippage
                : basePrice - slippage;
            
            return domain::OrderResult{
                .orderId = generateOrderId(),
                .status = domain::OrderStatus::FILLED,
                .executedPrice = domain::Money::fromDouble(fillPrice, "RUB"),
                .executedQuantity = request.quantity,
                .message = slippage > 0 ? "Filled with slippage" : "Filled",
                .timestamp = domain::Timestamp::now()
            };
        } else {
            // Limit order: проверяем можно ли исполнить сразу
            bool canFillNow = false;
            double limitPrice = request.price.toDouble();
            
            if (request.direction == domain::OrderDirection::BUY) {
                canFillNow = limitPrice >= quote.ask;
            } else {
                canFillNow = limitPrice <= quote.bid;
            }
            
            if (canFillNow) {
                return fillImmediately(request, quote);
            } else {
                // Добавляем в pending
                domain::Order order;
                order.id = generateOrderId();
                order.accountId = request.accountId;
                order.figi = request.figi;
                order.direction = request.direction;
                order.type = request.type;
                order.quantity = request.quantity;
                order.price = request.price;
                order.status = domain::OrderStatus::PENDING;
                
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    pendingOrders_[order.id] = order;
                }
                
                return domain::OrderResult{
                    .orderId = order.id,
                    .status = domain::OrderStatus::PENDING,
                    .message = "Limit order queued",
                    .timestamp = domain::Timestamp::now()
                };
            }
        }
    }
    
    domain::OrderResult fillPartially(
        const domain::OrderRequest& request,
        const PriceSimulator::Quote& quote,
        const MarketScenario& scenario)
    {
        int64_t filledQty = static_cast<int64_t>(request.quantity * scenario.partialFillRatio);
        filledQty = std::max(1L, filledQty);
        
        double fillPrice = (request.direction == domain::OrderDirection::BUY)
            ? quote.ask
            : quote.bid;
        
        return domain::OrderResult{
            .orderId = generateOrderId(),
            .status = (filledQty < request.quantity) 
                ? domain::OrderStatus::PARTIALLY_FILLED 
                : domain::OrderStatus::FILLED,
            .executedPrice = domain::Money::fromDouble(fillPrice, "RUB"),
            .executedQuantity = filledQty,
            .message = "Partially filled",
            .timestamp = domain::Timestamp::now()
        };
    }
    
    domain::OrderResult queueForDelayedFill(
        const domain::OrderRequest& request,
        const PriceSimulator::Quote& quote,
        const MarketScenario& scenario)
    {
        // TODO: Implement async delayed fill with std::async
        // Пока возвращаем PENDING
        return domain::OrderResult{
            .orderId = generateOrderId(),
            .status = domain::OrderStatus::PENDING,
            .message = "Order queued for delayed execution",
            .timestamp = domain::Timestamp::now()
        };
    }
    
    domain::OrderResult rejectOrder(const domain::OrderRequest& request, const std::string& reason) {
        return domain::OrderResult{
            .orderId = generateOrderId(),
            .status = domain::OrderStatus::REJECTED,
            .message = reason.empty() ? "Order rejected" : reason,
            .timestamp = domain::Timestamp::now()
        };
    }
    
    void publishFillEvent(const domain::Order& order, double fillPrice) {
        if (eventBus_) {
            domain::events::OrderFilledEvent event(
                order.id,
                order.accountId,
                order.figi,
                order.quantity,
                domain::Money::fromDouble(fillPrice, "RUB")
            );
            eventBus_->publish(event);
        }
    }
    
    void publishRejectEvent(const domain::Order& order, const std::string& reason) {
        // TODO: OrderRejectedEvent
    }
    
    std::string generateOrderId() {
        static std::atomic<uint64_t> counter{0};
        return "fake-order-" + std::to_string(++counter);
    }
};

} // namespace
```

### 4. Обновлённый FakeTinkoffAdapter

```cpp
namespace trading::adapters::secondary {

/**
 * @brief Улучшенный FakeTinkoffAdapter с поддержкой сценариев
 */
class FakeTinkoffAdapter : public ports::output::IBrokerGateway {
public:
    FakeTinkoffAdapter()
        : priceSimulator_(std::make_shared<PriceSimulator>())
        , orderProcessor_(std::make_shared<OrderProcessor>(priceSimulator_, nullptr))
    {
        initDefaultInstruments();
    }
    
    // ================================================================
    // КОНФИГУРАЦИЯ ДЛЯ ТЕСТОВ
    // ================================================================
    
    /**
     * @brief Установить сценарий для инструмента
     */
    void setScenario(const std::string& figi, const MarketScenario& scenario) {
        scenarios_[figi] = scenario;
        priceSimulator_->initInstrument(
            figi, 
            scenario.basePrice, 
            scenario.bidAskSpread, 
            scenario.volatility
        );
    }
    
    /**
     * @brief Установить глобальный сценарий (по умолчанию)
     */
    void setDefaultScenario(const MarketScenario& scenario) {
        defaultScenario_ = scenario;
    }
    
    /**
     * @brief Получить симулятор цен (для прямого управления)
     */
    PriceSimulator& priceSimulator() { return *priceSimulator_; }
    
    /**
     * @brief Получить процессор ордеров (для тестов)
     */
    OrderProcessor& orderProcessor() { return *orderProcessor_; }
    
    // ================================================================
    // IBrokerGateway INTERFACE
    // ================================================================
    
    domain::OrderResult placeOrder(
        const std::string& accountId,
        const domain::OrderRequest& request) override
    {
        auto scenario = getScenario(request.figi);
        return orderProcessor_->processOrder(request, scenario);
    }
    
    std::optional<domain::Quote> getQuote(const std::string& figi) override {
        auto q = priceSimulator_->getQuote(figi);
        
        domain::Quote quote;
        quote.figi = figi;
        quote.bid = domain::Money::fromDouble(q.bid, "RUB");
        quote.ask = domain::Money::fromDouble(q.ask, "RUB");
        quote.lastPrice = domain::Money::fromDouble(q.last, "RUB");
        quote.volume = q.volume;
        quote.timestamp = q.timestamp;
        
        return quote;
    }
    
    // ... остальные методы ...

private:
    std::shared_ptr<PriceSimulator> priceSimulator_;
    std::shared_ptr<OrderProcessor> orderProcessor_;
    
    MarketScenario defaultScenario_;
    std::unordered_map<std::string, MarketScenario> scenarios_;
    
    MarketScenario getScenario(const std::string& figi) {
        auto it = scenarios_.find(figi);
        return (it != scenarios_.end()) ? it->second : defaultScenario_;
    }
    
    void initDefaultInstruments() {
        // SBER
        priceSimulator_->initInstrument("BBG004730N88", 280.0, 0.001, 0.002);
        // GAZP
        priceSimulator_->initInstrument("BBG004730RP0", 160.0, 0.001, 0.003);
        // YNDX
        priceSimulator_->initInstrument("BBG006L8G4H1", 3500.0, 0.002, 0.004);
        // LKOH
        priceSimulator_->initInstrument("BBG004731032", 7200.0, 0.001, 0.002);
        // MGNT
        priceSimulator_->initInstrument("BBG004RVFCY3", 5500.0, 0.002, 0.003);
    }
};

} // namespace
```

## Примеры использования в тестах

### Базовый тест: Market order

```cpp
TEST(FakeBrokerTest, MarketOrderFillsImmediately) {
    auto broker = std::make_shared<FakeTinkoffAdapter>();
    
    // Устанавливаем цену
    broker->priceSimulator().setPrice("BBG004730N88", 280.0);
    
    // Размещаем market order
    domain::OrderRequest req;
    req.accountId = "acc-001";
    req.figi = "BBG004730N88";
    req.direction = domain::OrderDirection::BUY;
    req.type = domain::OrderType::MARKET;
    req.quantity = 10;
    
    auto result = broker->placeOrder(req.accountId, req);
    
    EXPECT_EQ(result.status, domain::OrderStatus::FILLED);
    EXPECT_EQ(result.executedQuantity, 10);
    EXPECT_NEAR(result.executedPrice.toDouble(), 280.0, 1.0);
}
```

### Тест: Limit order исполняется при движении цены

```cpp
TEST(FakeBrokerTest, LimitOrderFillsWhenPriceReaches) {
    auto broker = std::make_shared<FakeTinkoffAdapter>();
    
    // Настраиваем реалистичный сценарий
    MarketScenario scenario;
    scenario.fillBehavior = OrderFillBehavior::REALISTIC;
    scenario.basePrice = 280.0;
    broker->setScenario("BBG004730N88", scenario);
    
    // Размещаем buy limit ниже текущей цены
    domain::OrderRequest req;
    req.figi = "BBG004730N88";
    req.direction = domain::OrderDirection::BUY;
    req.type = domain::OrderType::LIMIT;
    req.price = domain::Money::fromDouble(275.0, "RUB");  // Ниже текущей 280
    req.quantity = 10;
    
    auto result = broker->placeOrder("acc-001", req);
    EXPECT_EQ(result.status, domain::OrderStatus::PENDING);
    
    // Двигаем цену вниз
    broker->priceSimulator().setPrice("BBG004730N88", 274.0);
    
    // Обрабатываем pending ордера
    broker->orderProcessor().processPendingOrders();
    
    // Проверяем что ордер исполнился (через event или getOrder)
}
```

### Тест: Slippage при большом объёме

```cpp
TEST(FakeBrokerTest, LargeOrderHasSlippage) {
    auto broker = std::make_shared<FakeTinkoffAdapter>();
    
    MarketScenario scenario;
    scenario.fillBehavior = OrderFillBehavior::REALISTIC;
    scenario.basePrice = 280.0;
    scenario.availableLiquidity = 1000;
    scenario.slippagePercent = 0.01;  // 1%
    broker->setScenario("BBG004730N88", scenario);
    
    // Большой ордер (больше 10% ликвидности)
    domain::OrderRequest req;
    req.figi = "BBG004730N88";
    req.direction = domain::OrderDirection::BUY;
    req.type = domain::OrderType::MARKET;
    req.quantity = 500;  // 50% ликвидности
    
    auto result = broker->placeOrder("acc-001", req);
    
    EXPECT_EQ(result.status, domain::OrderStatus::FILLED);
    // Цена должна быть выше из-за slippage
    EXPECT_GT(result.executedPrice.toDouble(), 280.0);
}
```

### Тест: Partial fill

```cpp
TEST(FakeBrokerTest, PartialFillScenario) {
    auto broker = std::make_shared<FakeTinkoffAdapter>();
    
    MarketScenario scenario;
    scenario.fillBehavior = OrderFillBehavior::PARTIAL;
    scenario.partialFillRatio = 0.5;  // Исполнится только 50%
    broker->setScenario("BBG004730N88", scenario);
    
    domain::OrderRequest req;
    req.figi = "BBG004730N88";
    req.quantity = 100;
    
    auto result = broker->placeOrder("acc-001", req);
    
    EXPECT_EQ(result.status, domain::OrderStatus::PARTIALLY_FILLED);
    EXPECT_EQ(result.executedQuantity, 50);
}
```

### Тест: Rejection

```cpp
TEST(FakeBrokerTest, RandomRejection) {
    auto broker = std::make_shared<FakeTinkoffAdapter>();
    
    MarketScenario scenario;
    scenario.fillBehavior = OrderFillBehavior::REALISTIC;
    scenario.rejectProbability = 1.0;  // 100% rejection
    scenario.rejectReason = "Insufficient funds";
    broker->setScenario("BBG004730N88", scenario);
    
    domain::OrderRequest req;
    req.figi = "BBG004730N88";
    req.quantity = 10;
    
    auto result = broker->placeOrder("acc-001", req);
    
    EXPECT_EQ(result.status, domain::OrderStatus::REJECTED);
    EXPECT_EQ(result.message, "Insufficient funds");
}
```

## Roadmap реализации

### Phase 1: MVP (текущий спринт)
- [ ] `MarketScenario` struct
- [ ] `PriceSimulator` базовая версия
- [ ] Интеграция в существующий `FakeTinkoffAdapter`

### Phase 2: Расширение
- [ ] `OrderProcessor` с pending очередью
- [ ] `processPendingOrders()` для limit-ордеров
- [ ] Публикация событий через EventBus

### Phase 3: Advanced
- [ ] Async delayed fills
- [ ] Более сложные модели ценообразования
- [ ] Симуляция стакана (order book)
- [ ] Тестовые сценарии: flash crash, gap, halt

## Заключение

Предложенная архитектура позволяет:
1. **Детерминированное тестирование** — установка конкретных цен и сценариев
2. **Реалистичные сценарии** — slippage, partial fills, rejections
3. **Интеграционные тесты** — limit-ордера исполняются при движении цены
4. **Стресс-тестирование** — симуляция высокой волатильности

Код разделён на компоненты (PriceSimulator, OrderProcessor), что упрощает тестирование и поддержку.
