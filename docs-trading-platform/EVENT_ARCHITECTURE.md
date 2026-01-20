# Event-Driven Architecture: Trading Platform

## Обзор

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         АРХИТЕКТУРА СОБЫТИЙ                                 │
└─────────────────────────────────────────────────────────────────────────────┘

  trading-service                RabbitMQ                    broker-service
       │                            │                              │
       │      order.create          │                              │
       │───────────────────────────▶│ trading.events ─────────────▶│
       │      order.cancel          │   (exchange)                 │
       │───────────────────────────▶│                              │
       │                            │                              │
       │                            │                              │
       │      order.created         │                              │
       │◀───────────────────────────│ broker.events ◀──────────────│
       │      order.filled          │   (exchange)                 │
       │◀───────────────────────────│                              │
       │      order.rejected        │                              │
       │◀───────────────────────────│                              │
       │      quote.updated         │                              │
       │◀───────────────────────────│                              │
       │      portfolio.updated     │                              │
       │◀───────────────────────────│                              │
```

---

## Exchanges

| Exchange | Type | Producer | Consumer |
|----------|------|----------|----------|
| `trading.events` | topic | trading-service | broker-service |
| `broker.events` | topic | broker-service | trading-service |

---

## События: trading.events

### `order.create`

```json
{
  "order_id": "ord-a1b2c3d4",
  "account_id": "acc-001",
  "figi": "BBG004730N88",
  "direction": "BUY",
  "type": "MARKET",
  "quantity": 10,
  "price": 265.50,
  "currency": "RUB",
  "timestamp": "2025-01-04T12:00:00Z"
}
```

### `order.cancel`

```json
{
  "order_id": "ord-a1b2c3d4",
  "account_id": "acc-001",
  "timestamp": "2025-01-04T12:05:00Z"
}
```

---

## События: broker.events

### `order.created`

```json
{"order_id": "ord-a1b2c3d4", "account_id": "acc-001", "figi": "BBG004730N88", "status": "PENDING", "timestamp": 1704369600000}
```

### `order.filled`

```json
{"order_id": "ord-a1b2c3d4", "account_id": "acc-001", "figi": "BBG004730N88", "status": "FILLED", "executed_lots": 10, "executed_price": 265.75, "currency": "RUB", "timestamp": 1704369601000}
```

### `order.partially_filled`

```json
{"order_id": "ord-a1b2c3d4", "status": "PARTIALLY_FILLED", "filled_lots": 5, "executed_price": 265.70, "timestamp": 1704369601000}
```

### `order.rejected`

```json
{"order_id": "ord-a1b2c3d4", "account_id": "acc-001", "figi": "BBG004730N88", "status": "REJECTED", "reason": "Insufficient funds", "timestamp": 1704369601000}
```

### `order.cancelled`

```json
{"order_id": "ord-a1b2c3d4", "account_id": "acc-001", "status": "CANCELLED", "timestamp": 1704369605000}
```

### `quote.updated`

```json
{"figi": "BBG004730N88", "bid": 265.50, "ask": 265.80, "last_price": 265.65, "currency": "RUB", "timestamp": 1704369600000}
```

### `portfolio.updated`

```json
{
  "account_id": "acc-001",
  "timestamp": 1704369601000,
  "cash": {"amount": 973500.00, "currency": "RUB"},
  "positions": [{"figi": "BBG004730N88", "quantity": 100, "average_price": 265.75, "current_price": 266.00}],
  "total_value": {"amount": 1000100.00, "currency": "RUB"}
}
```

---

## Жизненный цикл ордера

```
  Client              trading-service              broker-service
    │                       │                            │
    │ POST /orders          │                            │
    │──────────────────────▶│                            │
    │                       │ order.create ─────────────▶│
    │ 202 {PENDING}         │                            │
    │◀──────────────────────│           order.created ◀──│
    │                       │◀───────────────────────────│
    │                       │            order.filled ◀──│
    │                       │◀───────────────────────────│
    │                       │       portfolio.updated ◀──│
    │                       │◀───────────────────────────│
    │ GET /orders/{id}      │                            │
    │──────────────────────▶│                            │
    │ 200 {FILLED}          │                            │
    │◀──────────────────────│                            │
```

---

## Компоненты

### broker-service

| Компонент | Роль |
|-----------|------|
| `OrderCommandHandler` | Слушает order.create, order.cancel |
| `MarketDataPublisher` | Публикует quote.updated, portfolio.updated |
| `FakeBrokerAdapter` | Исполняет ордера |

### trading-service

| Компонент | Роль |
|-----------|------|
| `OrderService` | Публикует order.create, order.cancel |
| `TradingEventHandler` | Слушает все события от broker |

---

## HTTP API

| Операция | Способ |
|----------|--------|
| Создать ордер | RabbitMQ `order.create` |
| Отменить ордер | RabbitMQ `order.cancel` |
| Получить ордера | HTTP GET |
| Получить портфель | HTTP GET |
| Получить котировки | HTTP GET |

---

## Тестирование E2E

```javascript
// Postman: создаём ордер → ждём → проверяем статус
pm.test("Order eventually filled", function() {
    // 1. POST /orders → 202 {order_id, status: PENDING}
    // 2. setTimeout 1-2 сек
    // 3. GET /orders/{order_id} → status: FILLED
});
```

---

## Unit-тесты с InMemoryEventBus (планируется)

```cpp
class InMemoryEventBus : public IEventPublisher, public IEventConsumer {
    std::map<std::string, std::vector<EventHandler>> handlers_;
    
    void publish(const std::string& key, const std::string& msg) override {
        for (auto& h : handlers_[key]) h(key, msg);  // Синхронный вызов
    }
    
    void subscribe(const std::vector<std::string>& keys, EventHandler h) override {
        for (const auto& k : keys) handlers_[k].push_back(h);
    }
};

TEST(OrderFlow, CreateOrderViaEvents) {
    auto bus = std::make_shared<InMemoryEventBus>();
    
    // broker слушает
    bus->subscribe({"order.create"}, [&](auto, auto) {
        bus->publish("order.filled", R"({"order_id":"ord-1"})");
    });
    
    // trading слушает
    bool received = false;
    bus->subscribe({"order.filled"}, [&](auto, auto) { received = true; });
    
    // Act
    bus->publish("order.create", R"({"figi":"SBER"})");
    
    EXPECT_TRUE(received);
}
```

---

## Метрики

```
rabbitmq_events_published_total{routing_key="order.create",service="trading"} 150
rabbitmq_events_published_total{routing_key="order.filled",service="broker"} 142
rabbitmq_events_consumed_total{routing_key="order.filled",service="trading"} 142
```

---

*Последнее обновление: 2025-01-04*
