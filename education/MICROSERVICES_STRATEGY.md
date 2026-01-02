# Курсовой проект: Распил MVP на микросервисы

> **Версия:** 6.0 (FINAL)  
> **Дата:** 2025-01-02  
> **Статус:** Утверждён к реализации

---

## 1. Архитектура

```
                               ┌─────────────────────┐
                               │      Ingress        │
                               │   arch.homework     │
                               └──────────┬──────────┘
                                          │
            ┌─────────────────────────────┼─────────────────────────────┐
            │                             │                             │
            ▼                             ▼                             ▼
   ┌─────────────────┐           ┌─────────────────┐           ┌─────────────────┐
   │  Auth Service   │◀──REST────│ Trading Service │           │ Broker Service  │
   │     :8081       │  validate │     :8082       │           │     :8083       │
   └────────┬────────┘           └────────┬────────┘           └────────┬────────┘
            │                             │                             │
            │                             │      RabbitMQ               │
            │                             │  ┌─────────────────┐        │
            │                             └──▶  order.placed   ├────────▶
            │                             ┌──┤  order.executed │◀───────┘
            │                             │  │  order.partially│
            │                             │  │  order.rejected │
            │                             │  └─────────────────┘
            ▼                             ▼                             ▼
   ┌─────────────────┐           ┌─────────────────┐           ┌─────────────────┐
   │   auth_db       │           │   trading_db    │           │   broker_db     │
   └─────────────────┘           └─────────────────┘           └─────────────────┘
```

---

## 2. Правила распила

### 2.1 InMemory репозитории → только тесты

```
MVP:
  adapters/secondary/persistence/InMemory*Repository.hpp

Микросервисы:
  {service}/tests/mocks/InMemory*Repository.hpp
```

### 2.2 ICachePort выпиливаем

Используем кэш из библиотеки напрямую:

```cpp
#include <cache/Cache.hpp>
#include <cache/concurrency/ThreadSafeCache.hpp>
#include <cache/eviction/LRUPolicy.hpp>
#include <cache/expiration/GlobalTTL.hpp>

ThreadSafeCache<std::string, QuoteData> quotesCache_(
    Cache<std::string, QuoteData>(
        100,
        std::make_unique<LRUPolicy<std::string>>(),
        std::make_unique<GlobalTTL<std::string>>(std::chrono::seconds(2))
    )
);
```

### 2.3 Токен Tinkoff - упрощённое решение

```cpp
// Auth Service - при сохранении
std::string encode(const std::string& token) {
    // TODO: Production - RSA с публичным ключом Broker
    return base64_encode(token);
}

// Broker Service - при использовании
std::string decode(const std::string& encoded) {
    // TODO: Production - RSA с приватным ключом
    return base64_decode(encoded);
}
```

В README: "В production токен шифруется RSA. В учебной версии — base64."

---

## 3. Структура микросервисов

### 3.1 Auth Service

```
auth-service/
├── CMakeLists.txt
├── Dockerfile
├── config.json
├── include/
│   ├── AuthApp.hpp                              ← NEW
│   ├── adapters/
│   │   ├── primary/
│   │   │   ├── HealthHandler.hpp                ← из hw06
│   │   │   ├── MetricsHandler.hpp               ← из MVP
│   │   │   ├── LoginHandler.hpp                 ← из MVP
│   │   │   ├── LogoutHandler.hpp                ← из MVP
│   │   │   ├── RegisterHandler.hpp              ← из MVP
│   │   │   ├── ValidateTokenHandler.hpp         ← из MVP
│   │   │   ├── GetAccountsHandler.hpp           ← из MVP
│   │   │   ├── AddAccountHandler.hpp            ← из MVP
│   │   │   └── DeleteAccountHandler.hpp         ← из MVP
│   │   └── secondary/
│   │       ├── FakeJwtAdapter.hpp               ← из MVP
│   │       ├── PostgresUserRepository.hpp       ← из MVP
│   │       ├── PostgresAccountRepository.hpp    ← из MVP
│   │       ├── PostgresSessionRepository.hpp    ← NEW
│   │       └── DbSettings.hpp                   ← из hw08
│   ├── application/
│   │   ├── AuthService.hpp                      ← из MVP
│   │   └── AccountService.hpp                   ← из MVP
│   ├── domain/
│   │   ├── User.hpp                             ← из MVP
│   │   ├── Account.hpp                          ← из MVP (+tinkoff_token_encrypted)
│   │   ├── Session.hpp                          ← NEW
│   │   └── enums/
│   │       ├── AccountType.hpp                  ← из MVP
│   │       └── TokenType.hpp                    ← из MVP
│   └── ports/
│       ├── input/
│       │   ├── IAuthService.hpp                 ← из MVP
│       │   └── IAccountService.hpp              ← из MVP
│       └── output/
│           ├── IUserRepository.hpp              ← из MVP
│           ├── IAccountRepository.hpp           ← из MVP
│           ├── ISessionRepository.hpp           ← NEW
│           └── IJwtProvider.hpp                 ← из MVP
├── src/
│   ├── AuthApp.cpp
│   └── main.cpp
├── sql/
│   └── init.sql
└── tests/
    └── mocks/
        ├── InMemoryUserRepository.hpp           ← из MVP
        └── InMemoryAccountRepository.hpp        ← из MVP
```

### 3.2 Trading Service

```
trading-service/
├── CMakeLists.txt
├── Dockerfile
├── config.json
├── include/
│   ├── TradingApp.hpp                           ← NEW
│   ├── adapters/
│   │   ├── primary/
│   │   │   ├── HealthHandler.hpp                ← из hw08
│   │   │   ├── MetricsHandler.hpp               ← из MVP
│   │   │   ├── OrderHandler.hpp                 ← из MVP (адаптирован)
│   │   │   ├── PortfolioHandler.hpp             ← из MVP
│   │   │   ├── QuotesProxyHandler.hpp           ← NEW
│   │   │   ├── InstrumentsProxyHandler.hpp      ← NEW
│   │   │   └── IdempotentHandler.hpp            ← NEW (из hw09)
│   │   └── secondary/
│   │       ├── PostgresOrderRepository.hpp      ← из MVP (расширен)
│   │       ├── PostgresPortfolioRepository.hpp  ← NEW
│   │       ├── PostgresExecutionRepository.hpp  ← NEW
│   │       ├── PostgresBalanceRepository.hpp    ← NEW
│   │       ├── PostgresIdempotencyRepository.hpp← NEW (из hw09)
│   │       ├── HttpAuthClient.hpp               ← NEW
│   │       ├── HttpBrokerClient.hpp             ← NEW
│   │       ├── RabbitMQAdapter.hpp              ← из hw08
│   │       ├── DbSettings.hpp                   ← из hw08
│   │       ├── RabbitMQSettings.hpp             ← из hw08
│   │       ├── AuthClientSettings.hpp           ← NEW
│   │       └── BrokerClientSettings.hpp         ← NEW
│   ├── application/
│   │   ├── OrderService.hpp                     ← из MVP (Saga + резервирование)
│   │   ├── PortfolioService.hpp                 ← из MVP
│   │   ├── OrderEventHandler.hpp                ← NEW
│   │   └── ResponseCapture.hpp                  ← NEW (из hw09)
│   ├── domain/
│   │   ├── Order.hpp                            ← из MVP (+filled_quantity)
│   │   ├── Execution.hpp                        ← NEW
│   │   ├── Position.hpp                         ← из MVP
│   │   ├── Portfolio.hpp                        ← из MVP
│   │   ├── Balance.hpp                          ← NEW
│   │   ├── IdempotencyRecord.hpp                ← NEW (из hw09)
│   │   └── enums/
│   │       ├── OrderDirection.hpp               ← из MVP
│   │       ├── OrderStatus.hpp                  ← из MVP (+PARTIALLY_FILLED)
│   │       └── OrderType.hpp                    ← из MVP
│   └── ports/
│       ├── input/
│       │   ├── IOrderService.hpp                ← из MVP
│       │   └── IPortfolioService.hpp            ← из MVP
│       └── output/
│           ├── IOrderRepository.hpp             ← из MVP
│           ├── IPortfolioRepository.hpp         ← из MVP
│           ├── IExecutionRepository.hpp         ← NEW
│           ├── IBalanceRepository.hpp           ← NEW
│           ├── IIdempotencyRepository.hpp       ← NEW
│           ├── IAuthClient.hpp                  ← NEW
│           ├── IBrokerClient.hpp                ← NEW
│           ├── IEventPublisher.hpp              ← из hw08
│           └── IEventConsumer.hpp               ← из hw08
├── src/
│   ├── TradingApp.cpp
│   ├── RabbitMQAdapter.cpp
│   └── main.cpp
├── sql/
│   └── init.sql
└── tests/
    └── mocks/
        ├── InMemoryOrderRepository.hpp          ← из MVP
        └── InMemoryPortfolioRepository.hpp      ← из MVP
```

### 3.3 Broker Service

```
broker-service/
├── CMakeLists.txt
├── Dockerfile
├── config.json
├── include/
│   ├── BrokerApp.hpp                            ← NEW
│   ├── adapters/
│   │   ├── primary/
│   │   │   ├── HealthHandler.hpp                ← из hw08
│   │   │   ├── MetricsHandler.hpp               ← из MVP
│   │   │   ├── QuotesHandler.hpp                ← NEW
│   │   │   └── InstrumentsHandler.hpp           ← NEW
│   │   └── secondary/
│   │       ├── PostgresInstrumentRepository.hpp ← NEW
│   │       ├── PostgresQuoteRepository.hpp      ← NEW
│   │       ├── PostgresBrokerOrderRepository.hpp← NEW
│   │       ├── PostgresBrokerPositionRepository.hpp ← NEW
│   │       ├── PostgresBrokerBalanceRepository.hpp  ← NEW
│   │       ├── RabbitMQAdapter.hpp              ← из hw08
│   │       ├── DbSettings.hpp                   ← из hw08
│   │       └── RabbitMQSettings.hpp             ← из hw08
│   ├── application/
│   │   ├── OrderExecutor.hpp                    ← NEW (из MVP OrderProcessor)
│   │   ├── PriceSimulator.hpp                   ← из MVP
│   │   ├── OrderEventHandler.hpp                ← NEW
│   │   └── QuoteService.hpp                     ← NEW
│   ├── domain/
│   │   ├── Instrument.hpp                       ← из MVP (+min_price_increment)
│   │   ├── Quote.hpp                            ← из MVP
│   │   ├── BrokerOrder.hpp                      ← NEW
│   │   ├── BrokerPosition.hpp                   ← NEW
│   │   └── BrokerBalance.hpp                    ← NEW
│   └── ports/
│       ├── input/
│       │   └── IQuoteService.hpp                ← NEW
│       └── output/
│           ├── IInstrumentRepository.hpp        ← NEW
│           ├── IQuoteRepository.hpp             ← NEW
│           ├── IBrokerOrderRepository.hpp       ← NEW
│           ├── IBrokerPositionRepository.hpp    ← NEW
│           ├── IBrokerBalanceRepository.hpp     ← NEW
│           ├── IEventPublisher.hpp              ← из hw08
│           └── IEventConsumer.hpp               ← из hw08
├── src/
│   ├── BrokerApp.cpp
│   ├── RabbitMQAdapter.cpp
│   └── main.cpp
├── sql/
│   └── init.sql
└── tests/
    └── OrderExecutorTest.cpp
```

---

## 4. База данных

### 4.1 Auth DB

```sql
CREATE TABLE users (
    user_id VARCHAR(64) PRIMARY KEY,
    username VARCHAR(64) UNIQUE NOT NULL,
    email VARCHAR(128) UNIQUE NOT NULL,
    password_hash VARCHAR(256) NOT NULL,
    created_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE accounts (
    account_id VARCHAR(64) PRIMARY KEY,
    user_id VARCHAR(64) REFERENCES users(user_id),
    name VARCHAR(64) NOT NULL,
    type VARCHAR(16) NOT NULL,
    tinkoff_token_encrypted TEXT,
    created_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE sessions (
    session_id VARCHAR(64) PRIMARY KEY,
    user_id VARCHAR(64) REFERENCES users(user_id),
    jwt_token VARCHAR(512) NOT NULL,
    expires_at TIMESTAMP NOT NULL,
    created_at TIMESTAMP DEFAULT NOW()
);
```

### 4.2 Trading DB

```sql
CREATE TABLE orders (
    order_id VARCHAR(64) PRIMARY KEY,
    account_id VARCHAR(64) NOT NULL,
    user_id VARCHAR(64) NOT NULL,
    figi VARCHAR(12) NOT NULL,
    direction VARCHAR(4) NOT NULL,
    quantity INTEGER NOT NULL,
    filled_quantity INTEGER DEFAULT 0,
    price BIGINT NOT NULL,
    order_type VARCHAR(8) NOT NULL,
    status VARCHAR(20) NOT NULL,
    reject_reason VARCHAR(256),
    executed_price BIGINT,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE executions (
    execution_id VARCHAR(64) PRIMARY KEY,
    order_id VARCHAR(64) REFERENCES orders(order_id),
    quantity INTEGER NOT NULL,
    price BIGINT NOT NULL,
    executed_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE portfolio_positions (
    position_id VARCHAR(64) PRIMARY KEY,
    account_id VARCHAR(64) NOT NULL,
    user_id VARCHAR(64) NOT NULL,
    figi VARCHAR(12) NOT NULL,
    quantity INTEGER NOT NULL,
    avg_price BIGINT NOT NULL,
    updated_at TIMESTAMP DEFAULT NOW(),
    UNIQUE(account_id, figi)
);

CREATE TABLE balances (
    account_id VARCHAR(64) PRIMARY KEY,
    currency VARCHAR(3) NOT NULL DEFAULT 'RUB',
    available BIGINT NOT NULL DEFAULT 0,
    reserved BIGINT NOT NULL DEFAULT 0,
    updated_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE idempotency_keys (
    idempotency_key VARCHAR(64) PRIMARY KEY,
    order_id VARCHAR(64),
    response_status INTEGER,
    response_body TEXT,
    created_at TIMESTAMP DEFAULT NOW()
);
```

### 4.3 Broker DB

```sql
CREATE TABLE instruments (
    figi VARCHAR(12) PRIMARY KEY,
    ticker VARCHAR(12) NOT NULL,
    name VARCHAR(128) NOT NULL,
    currency VARCHAR(3) NOT NULL,
    lot_size INTEGER NOT NULL,
    min_price_increment BIGINT NOT NULL
);

CREATE TABLE quotes (
    figi VARCHAR(12) PRIMARY KEY REFERENCES instruments(figi),
    bid BIGINT NOT NULL,
    ask BIGINT NOT NULL,
    last_price BIGINT NOT NULL,
    updated_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE broker_orders (
    order_id VARCHAR(64) PRIMARY KEY,
    account_id VARCHAR(64) NOT NULL,
    figi VARCHAR(12) NOT NULL,
    direction VARCHAR(4) NOT NULL,
    quantity INTEGER NOT NULL,
    filled_quantity INTEGER DEFAULT 0,
    price BIGINT NOT NULL,
    order_type VARCHAR(8) NOT NULL,
    status VARCHAR(20) NOT NULL,
    reject_reason VARCHAR(256),
    received_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE broker_positions (
    account_id VARCHAR(64) NOT NULL,
    figi VARCHAR(12) NOT NULL,
    quantity INTEGER NOT NULL,
    avg_price BIGINT NOT NULL,
    updated_at TIMESTAMP DEFAULT NOW(),
    PRIMARY KEY(account_id, figi)
);

CREATE TABLE broker_balances (
    account_id VARCHAR(64) PRIMARY KEY,
    currency VARCHAR(3) NOT NULL DEFAULT 'RUB',
    available BIGINT NOT NULL DEFAULT 0,
    reserved BIGINT NOT NULL DEFAULT 0,
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Seed data
INSERT INTO instruments (figi, ticker, name, currency, lot_size, min_price_increment) VALUES
('BBG004730N88', 'SBER', 'Сбербанк', 'RUB', 10, 100),
('BBG004730RP0', 'GAZP', 'Газпром', 'RUB', 10, 100),
('BBG004731032', 'LKOH', 'Лукойл', 'RUB', 1, 500),
('BBG004731354', 'ROSN', 'Роснефть', 'RUB', 1, 100),
('BBG004S68614', 'ALRS', 'Алроса', 'RUB', 10, 10);

INSERT INTO quotes (figi, bid, ask, last_price) VALUES
('BBG004730N88', 26500, 26550, 26525),
('BBG004730RP0', 15800, 15850, 15820),
('BBG004731032', 710000, 710500, 710200),
('BBG004731354', 57500, 57600, 57550),
('BBG004S68614', 7200, 7250, 7220);
```

---

## 5. Резервирование денег

```
CREATE ORDER (BUY 100 лотов @ 265₽)
├── cost = quantity × lot_size × price
├── CHECK: available >= cost
├── RESERVE: available -= cost, reserved += cost
└── status = PENDING

ORDER EXECUTED
├── reserved -= cost
└── Акции в portfolio_positions

ORDER REJECTED / CANCELLED
├── available += cost, reserved -= cost
└── Деньги вернулись

ORDER PARTIALLY_FILLED (50 из 100)
├── filled_cost = filled × lot_size × price
├── reserved -= filled_cost (ушло на акции)
└── reserved остаток ждёт исполнения
```

---

## 6. RabbitMQ и Saga

### 6.1 События

| Событие | Publisher | Consumer |
|---------|-----------|----------|
| `order.placed` | Trading | Broker |
| `order.cancelled` | Trading | Broker |
| `order.executed` | Broker | Trading |
| `order.partially_filled` | Broker | Trading |
| `order.rejected` | Broker | Trading |

### 6.2 Статусы ордера

```
PENDING → PARTIALLY_FILLED → FILLED
       └→ REJECTED
       └→ CANCELLED
```

### 6.3 Recovery Broker

```cpp
void BrokerApp::recoverPendingOrders() {
    auto pending = orderRepo_->findByStatus({"RECEIVED", "PROCESSING"});
    for (auto& order : pending) {
        orderExecutor_->continueExecution(order);
    }
}
```

---

## 7. Кэширование (Trading Service)

| Что | TTL | Тип |
|-----|-----|-----|
| quotes | 2 сек | `ThreadSafeCache<string, QuoteData>` |
| instruments | 5 мин | `ThreadSafeCache<string, Instrument>` |
| token validation | 30 сек | `ThreadSafeCache<string, bool>` |

---

## 8. Метрики

### 8.1 Все сервисы (стандартные)

```cpp
// Счётчик запросов
http_requests_total{service="auth", method="POST", path="/api/v1/auth/login", status="200"}

// Гистограмма latency
http_request_duration_seconds{service="auth", method="POST", path="/api/v1/auth/login"}
```

### 8.2 Trading Service (бизнес-метрики)

```cpp
// Количество ордеров по статусам
orders_total{direction="BUY", status="FILLED"}
orders_total{direction="SELL", status="REJECTED"}
```

### 8.3 Broker Service (бизнес-метрики)

```cpp
// Исполненные ордера
orders_executed_total{direction="BUY"}
orders_executed_total{direction="SELL"}
```

### 8.4 Реализация (из MVP)

```cpp
class PrometheusMetrics {
public:
    void incrementHttpRequests(const std::string& method, 
                               const std::string& path, 
                               int status);
    void observeHttpDuration(const std::string& method,
                             const std::string& path,
                             double seconds);
    void incrementOrders(const std::string& direction,
                         const std::string& status);
    std::string serialize();  // для /metrics endpoint
};
```

---

## 9. Postman тесты (~32)

| # | Сценарий | Тестов |
|---|----------|--------|
| 0 | Health Checks | 3 |
| 1 | Auth Flow | 5 |
| 2 | Instruments & Quotes | 3 |
| 3 | Saga Success | 4 |
| 4 | Saga Failure | 4 |
| 5 | Partial Fill | 5 |
| 6 | Cancel Order | 3 |
| 7 | Idempotency | 5 |

---

## 10. План реализации

### День 1 (сегодня): ~9-10 часов

| # | Задача | Время |
|---|--------|-------|
| 1.1 | k8s/ структура + secrets | 30 мин |
| 1.2 | PostgreSQL (3 БД) + init.sql | 1 час |
| 1.3 | RabbitMQ | 30 мин |
| 1.4 | Auth Service | 3 часа |
| 1.5 | Broker Service | 3 часа |
| 1.6 | Ingress | 30 мин |
| 1.7 | Postman тесты 0, 1, 2 | 1 час |

### День 2 (завтра): ~12-13 часов

| # | Задача | Время |
|---|--------|-------|
| 2.1 | Trading Service | 3 часа |
| 2.2 | Saga + Резервирование | 2 часа |
| 2.3 | Partial Fill | 1 час |
| 2.4 | Idempotency | 1 час |
| 2.5 | Postman тесты 3-7 | 1.5 часа |
| 2.6 | Prometheus + Grafana | 1 час |
| 2.7 | UI (минимум) | 1.5 часа |
| 2.8 | Презентация | 1.5 часа |
| 2.9 | Видео | 1 час |

---

## 11. Чеклист готовности

- [ ] Auth Service в K8s
- [ ] Broker Service в K8s
- [ ] Trading Service в K8s
- [ ] Ingress работает
- [ ] Saga success (PENDING → FILLED)
- [ ] Saga failure (PENDING → REJECTED)
- [ ] Partial fill (PENDING → PARTIALLY_FILLED → FILLED)
- [ ] Резервирование денег работает
- [ ] Idempotency работает
- [ ] Postman 32 теста, 0 failures
- [ ] Prometheus метрики
- [ ] Grafana дашборд
- [ ] UI минимальный
- [ ] Презентация
- [ ] Видео
