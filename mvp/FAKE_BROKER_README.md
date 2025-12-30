# Enhanced FakeBroker - Реализация

## Обзор

Реализована полная система симуляции брокера для тестирования торговой платформы:

```
include/adapters/secondary/broker/
├── MarketScenario.hpp      # Конфигурация сценариев исполнения
├── PriceSimulator.hpp      # Генератор цен (random walk)
├── OrderProcessor.hpp      # Обработчик ордеров
├── BackgroundTicker.hpp    # Фоновый поток симуляции
├── EnhancedFakeBroker.hpp  # Интегрированный брокер
└── FakeBrokerAdapter.hpp   # Адаптер для IBrokerGateway + IEventBus
```

## Архитектура

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

### 1. MarketScenario
Конфигурация поведения рынка для инструмента.

**Режимы исполнения:**
| Режим | Описание |
|-------|----------|
| `IMMEDIATE` | Мгновенное исполнение по текущей цене |
| `REALISTIC` | Market сразу, limit в pending |
| `PARTIAL` | Частичное исполнение |
| `DELAYED` | Отложенное исполнение |
| `ALWAYS_REJECT` | Всегда отклонять |

**Factory methods:**
```cpp
MarketScenario::immediate(280.0)
MarketScenario::realistic(280.0, 0.001, 0.002)  // basePrice, spread, volatility
MarketScenario::lowLiquidity(280.0, 100)        // slippage при большом объёме
MarketScenario::partialFill(280.0, 0.5)         // 50% исполнение
MarketScenario::alwaysReject("Market closed")
MarketScenario::highVolatility(280.0, 0.1)      // 10% волатильность
```

### 2. PriceSimulator
Генератор цен с geometric Brownian motion.

```cpp
auto simulator = std::make_shared<PriceSimulator>(42);  // Seed для детерминизма

// Инициализация инструмента
simulator->initInstrument("SBER", 280.0, 0.001, 0.002);

// Получение котировки
auto quote = simulator->getQuote("SBER");  // bid, ask, last, volume, timestamp

// Симуляция
simulator->tick("SBER");           // Один тик
simulator->simulate("SBER", 100);  // 100 тиков

// Управление ценой (для тестов)
simulator->setPrice("SBER", 300.0);
simulator->movePrice("SBER", 10.0);
simulator->movePricePercent("SBER", -5.0);
```

### 3. OrderProcessor
Обработка ордеров со сценариями.

```cpp
auto processor = std::make_shared<OrderProcessor>(priceSimulator);

// Callback для pending fills
processor->setFillCallback([](const OrderFillEvent& e) {
    std::cout << "Order filled: " << e.orderId << " @ " << e.price << "\n";
});

// Обработка ордера
OrderRequest req;
req.figi = "SBER";
req.direction = Direction::BUY;
req.type = Type::LIMIT;
req.quantity = 10;
req.price = 270.0;

auto result = processor->processOrder(req, scenario);
// result.status == Status::PENDING для limit below ask

// Обработка pending при изменении цены
priceSimulator->setPrice("SBER", 265.0);
int filled = processor->processPendingOrders();

// Force operations (для тестов)
processor->forceFillOrder(orderId, 275.0);
processor->forceRejectOrder(orderId, "Manual rejection");
processor->cancelOrder(orderId);
```

### 4. BackgroundTicker
Фоновый поток для автоматической симуляции.

```cpp
BackgroundTicker ticker(priceSimulator, orderProcessor);

// Добавление инструментов
ticker.addInstrument("SBER");
ticker.addInstrument("GAZP");

// Callback на обновление котировок
ticker.setQuoteCallback([](const QuoteUpdate& q) {
    eventBus->publish(QuoteUpdatedEvent(q));
});

// Запуск/остановка
ticker.start(100ms);  // Тик каждые 100мс
ticker.stop();

// Ручной тик (для тестов)
ticker.manualTick();
```

### 5. EnhancedFakeBroker
Интегрированный брокер со всеми возможностями.

```cpp
EnhancedFakeBroker broker(42);  // Seed для детерминизма

// Аккаунты
broker.registerAccount("acc-001", "token", 1000000.0);
broker.setCash("acc-001", 500000.0);

// Сценарии
broker.setScenario("SBER", MarketScenario::realistic(280.0));
broker.setDefaultScenario(MarketScenario::immediate());

// Симуляция
broker.startSimulation(100ms);
broker.stopSimulation();

// События
broker.setQuoteUpdateCallback([](const BrokerQuoteUpdateEvent& e) { ... });
broker.setOrderFillCallback([](const BrokerOrderFillEvent& e) { ... });

// Ордера
BrokerOrderRequest req{...};
auto result = broker.placeOrder("acc-001", req);

// Портфель
auto portfolio = broker.getPortfolio("acc-001");
std::cout << "Total: " << portfolio->totalValue() << "\n";
```

### 6. FakeBrokerAdapter
Адаптер для интеграции с системой (IBrokerGateway + IEventBus).

```cpp
auto eventBus = std::make_shared<InMemoryEventBus>();
auto adapter = std::make_shared<FakeBrokerAdapter>(eventBus);

// Публикует события автоматически:
// - QuoteUpdatedEvent при изменении котировок
// - OrderCreatedEvent при создании ордера
// - OrderFilledEvent при исполнении
// - OrderCancelledEvent при отмене

// Использование через DI
auto injector = di::make_injector(
    di::bind<ports::output::IBrokerGateway>.to<FakeBrokerAdapter>(),
    di::bind<ports::output::IEventBus>.to<InMemoryEventBus>()
);
```

## Тесты

**143 unit-теста, все проходят:**

| Компонент | Тестов | Покрытие |
|-----------|--------|----------|
| MarketScenario | 17 | Factory methods, copy/move semantics |
| PriceSimulator | 32 | Quotes, tick, price manipulation, thread safety |
| OrderProcessor | 39 | All scenarios, pending, callbacks, thread safety |
| BackgroundTicker | 19 | Lifecycle, callbacks, thread safety |
| EnhancedFakeBroker | 36 | Integration, portfolio, orders, scenarios |

Время выполнения: ~1.4 секунды

## Использование в Boost.DI

```cpp
auto injector = di::make_injector(
    // FakeBrokerAdapter вместо TinkoffGrpcAdapter для тестов
    di::bind<ports::output::IBrokerGateway>.to<FakeBrokerAdapter>(),
    di::bind<ports::output::IEventBus>.to<InMemoryEventBus>(),
    // Или RabbitMQEventBus для микросервисов
    // di::bind<ports::output::IEventBus>.to<RabbitMQEventBus>()
);

auto marketService = injector.create<std::shared_ptr<MarketService>>();
auto orderService = injector.create<std::shared_ptr<OrderService>>();
```

## Примеры использования в тестах

### Базовый тест: Market order

```cpp
TEST(FakeBrokerTest, MarketOrderFillsImmediately) {
    auto broker = std::make_shared<SimpleBrokerGatewayAdapter>();
    
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
    auto broker = std::make_shared<SimpleBrokerGatewayAdapter>();
    
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
    auto broker = std::make_shared<SimpleBrokerGatewayAdapter>();
    
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
    auto broker = std::make_shared<SimpleBrokerGatewayAdapter>();
    
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
    auto broker = std::make_shared<SimpleBrokerGatewayAdapter>();
    
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

## Демонстрация для защиты

1. **Monolith**: Все сервисы используют один `FakeBrokerAdapter` с `InMemoryEventBus`
2. **Microservices**: Broker Service использует `FakeBrokerAdapter`, публикует события в `RabbitMQEventBus`

```
┌─────────────────┐     RabbitMQ      ┌─────────────────┐
│  Broker Service │ ───────────────▶  │ Trading Service │
│                 │  quote.updated    │                 │
│ FakeBrokerAdapter│  order.filled    │   Strategies    │
│     ↓           │                   │       ↓         │
│ EnhancedFake-   │                   │  OrderService   │
│    Broker       │ ◀───────────────  │                 │
└─────────────────┘   order.create    └─────────────────┘
```
## Заключение

Предложенная архитектура позволяет:
1. **Детерминированное тестирование** — установка конкретных цен и сценариев
2. **Реалистичные сценарии** — slippage, partial fills, rejections
3. **Интеграционные тесты** — limit-ордера исполняются при движении цены
4. **Стресс-тестирование** — симуляция высокой волатильности

Код разделён на компоненты (PriceSimulator, OrderProcessor), что упрощает тестирование и поддержку.

## Roadmap реализации

### Phase 1: MVP
- [v] `MarketScenario` struct
- [v] `PriceSimulator` базовая версия
- [v] Интеграция в существующий `SimpleBrokerGatewayAdapter`

### Phase 2: Расширение
- [v] `OrderProcessor` с pending очередью
- [v] `processPendingOrders()` для limit-ордеров
- [v] Публикация событий через EventBus

### Phase 3: Advanced
- [ ] Async delayed fills
- [ ] Более сложные модели ценообразования
- [ ] Симуляция стакана (order book)
- [ ] Тестовые сценарии: flash crash, gap, halt

