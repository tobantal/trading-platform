# Биржевая торговля — Trading Platform MVP

## Курс: Microservice Architecture (OTUS)
## Версия: 2.1 (Актуальные требования для MVP)
## Дата: 16 декабря 2025

---

# Содержание

1. [Общее описание проекта](#1-общее-описание-проекта)
2. [Скоуп MVP](#2-скоуп-mvp)
3. [Гексагональная архитектура](#3-гексагональная-архитектура)
4. [Доменные сущности](#4-доменные-сущности)
5. [Порты (Interfaces)](#5-порты-interfaces)
6. [Адаптеры](#6-адаптеры)
7. [Event Bus](#7-event-bus)
8. [Пользовательские сценарии MVP](#8-пользовательские-сценарии-mvp)
9. [API спецификация](#9-api-спецификация)
10. [Схема данных](#10-схема-данных)
11. [Технологический стек](#11-технологический-стек)
12. [Структура проекта](#12-структура-проекта)
13. [Критерии приёмки](#13-критерии-приёмки)

---

# 1. Общее описание проекта

## 1.1 Цель проекта

Разработка **production-ready платформы** для алгоритмической торговли на Московской фондовой бирже (MOEX) через Tinkoff Invest API.

> ⚠️ **Важно**: Это не учебный проект "на выброс". Система проектируется для реальной торговли в будущем. Защита проводится на Sandbox, но архитектура готова к Production.

## 1.2 Ключевые принципы

| Принцип | Описание |
|---------|----------|
| **🏗️ Гексагональная архитектура** | Бизнес-логика изолирована от инфраструктуры через порты и адаптеры |
| **📦 Модульный монолит → Микросервисы** | Система может работать как монолит или как набор микросервисов |
| **🧪 Testability First** | Fake адаптеры позволяют тестировать без внешних зависимостей |
| **🔧 Расширяемость** | Легко добавлять новые стратегии, брокеров, инструменты |

## 1.3 Что уже реализовано (HelloWorld)

```
✅ Библиотеки подключены через FetchContent:
   └─ cpp-http-server (Boost.Beast + Boost.DI)
   └─ cpp-cache (LRU cache)

✅ Docker Compose работает:
   └─ backend (C++17)
   └─ nginx (reverse proxy)
   └─ PostgreSQL
   └─ Prometheus

✅ REST API endpoints:
   └─ GET /api/v1/health
   └─ GET /metrics
   └─ GET /echo
```

---

# 2. Скоуп MVP

## 2.1 Что входит в MVP

| Функционал | Статус | Примечание |
|------------|:------:|------------|
| **Торговля акциями** | ✅ MVP | Только акции |
| **Одна стратегия (SMA Crossover)** | ✅ MVP | Базовая стратегия |
| **Sandbox режим** | ✅ MVP | Fake Tinkoff API |
| **Авторизация JWT** | ✅ MVP | fake-jwt-server |
| **Просмотр портфеля** | ✅ MVP | Позиции, P&L |
| **Размещение ордеров** | ✅ MVP | Market, Limit |
| **Event Bus** | ✅ MVP | In-memory (→ RabbitMQ в Education) |
| **Простой Web UI** | ✅ MVP | HTML + JS + Bootstrap |

## 2.2 Что НЕ входит в MVP (Future)

| Функционал | Этап | Причина |
|------------|------|---------|
| Торговля облигациями | Education | Усложняет расчёты |
| Торговля фьючерсами | Production | Другая маржинальность |
| Реальный Tinkoff API | Production | Требует gRPC + протобуфы |
| ZITADEL/Keycloak | Education | fake-jwt-server достаточно |
| WebSocket real-time | Education | REST polling для MVP |
| RabbitMQ | Education | In-memory Event Bus для MVP |
| Kubernetes | Production | Docker Compose для MVP |
| Множество стратегий | Education | Одна SMA достаточно |

## 2.3 Fake Tinkoff API

Для MVP создаём **SimpleBrokerGatewayAdapter** который:
- Эмулирует поведение реального API
- Возвращает захардкоженные/рандомные котировки
- Позволяет тестировать логику без реального брокера
- Имеет тот же интерфейс (порт) что и будущий реальный адаптер

**Инструменты (5 штук):**

| FIGI | Ticker | Название | Лотность | Базовая цена |
|------|--------|----------|----------|--------------|
| BBG004730N88 | SBER | Сбербанк | 1 | 265 ₽ |
| BBG004731032 | LKOH | Лукойл | 1 | 7100 ₽ |
| BBG004731354 | GAZP | Газпром | 10 | 165 ₽ |
| BBG004S681W1 | VTBR | ВТБ | 10000 | 0.024 ₽ |
| BBG006L8G4H1 | YNDX | Яндекс | 1 | 3800 ₽ |

---

# 3. Гексагональная архитектура

## 3.1 Общая схема

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│                        ГЕКСАГОНАЛЬНАЯ АРХИТЕКТУРА                           │
│                           (Ports & Adapters)                                │
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                     PRIMARY ADAPTERS (Driving)                      │   │
│   │                                                                      │   │
│   │   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐            │   │
│   │   │ REST API     │   │ WebSocket    │   │   CLI        │            │   │
│   │   │ Controllers  │   │ (future)     │   │  (future)    │            │   │
│   │   └──────┬───────┘   └──────┬───────┘   └──────┬───────┘            │   │
│   └──────────┼──────────────────┼──────────────────┼────────────────────┘   │
│              │                  │                  │                        │
│              ▼                  ▼                  ▼                        │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                        INPUT PORTS                                  │   │
│   │                                                                      │   │
│   │   IAuthService   IMarketService   IOrderService   IStrategyService  │   │
│   │   IPortfolioService   IAccountService                               │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    ▼                                        │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                                                                      │   │
│   │                      ██████████████████████                          │   │
│   │                      █                    █                          │   │
│   │                      █   DOMAIN CORE      █                          │   │
│   │                      █                    █                          │   │
│   │                      █  • Entities        █                          │   │
│   │                      █  • Value Objects   █                          │   │
│   │                      █  • Domain Services █                          │   │
│   │                      █  • Business Rules  █                          │   │
│   │                      █                    █                          │   │
│   │                      ██████████████████████                          │   │
│   │                                                                      │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    ▼                                        │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                        OUTPUT PORTS                                 │   │
│   │                                                                      │   │
│   │  IOrderRepository   IBrokerGateway   IUserRepository   ICache       │   │
│   │  IStrategyRepository   IJwtProvider   IEventBus                     │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│              │              │              │              │                  │
│              ▼              ▼              ▼              ▼                  │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                    SECONDARY ADAPTERS (Driven)                      │   │
│   │                                                                      │   │
│   │   ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌────────────┐ │   │
│   │   │PostgreSQL    │ │FakeTinkoff   │ │fake-jwt      │ │InMemory    │ │   │
│   │   │Repository    │ │Adapter       │ │Adapter       │ │EventBus    │ │   │
│   │   └──────────────┘ └──────────────┘ └──────────────┘ └────────────┘ │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

# 4. Доменные сущности

## 4.1 Core Entities

### Account (Брокерский счёт)
```cpp
struct Account {
    std::string id;              // UUID
    std::string userId;          // FK to users
    std::string name;            // "Tinkoff Sandbox"
    AccountType type;            // SANDBOX / PRODUCTION
    std::string accessToken;     // Encrypted in DB
    bool isActive;
};
```

### Quote (Котировка)
```cpp
struct Quote {
    std::string figi;            // "BBG004730N88" (SBER)
    std::string ticker;          // "SBER"
    Money lastPrice;             // 265.50 RUB
    Money bidPrice;              // 265.45 RUB
    Money askPrice;              // 265.55 RUB
    Timestamp updatedAt;
};
```

### Position (Позиция в портфеле)
```cpp
struct Position {
    std::string figi;
    std::string ticker;
    int64_t quantity;            // В лотах
    Money averagePrice;          // Средняя цена покупки
    Money currentPrice;          // Текущая цена
    Money pnl;                   // Profit & Loss
    double pnlPercent;           // P&L в процентах
};
```

### Order (Ордер)
```cpp
struct Order {
    std::string id;
    std::string accountId;
    std::string figi;
    OrderDirection direction;    // BUY / SELL
    OrderType type;              // MARKET / LIMIT
    int64_t quantity;            // В лотах
    Money price;                 // Для LIMIT ордеров
    OrderStatus status;          // PENDING / FILLED / CANCELLED / REJECTED
    Timestamp createdAt;
    Timestamp updatedAt;
};
```

### Strategy (Торговая стратегия)
```cpp
struct Strategy {
    std::string id;
    std::string accountId;
    std::string name;
    StrategyType type;           // SMA_CROSSOVER
    std::string config;          // JSON с параметрами
    StrategyStatus status;       // STOPPED / RUNNING / ERROR
    std::string figi;            // На какой инструмент
    Timestamp createdAt;
};
```

### Signal (Торговый сигнал)
```cpp
struct Signal {
    std::string id;
    std::string strategyId;
    SignalType type;             // BUY / SELL / HOLD
    std::string figi;
    Money price;
    std::string reason;          // "SMA10 crossed above SMA30"
    Timestamp timestamp;
};
```

## 4.2 Value Objects

```cpp
// Денежная сумма (как в Tinkoff API)
struct Money {
    int64_t units;               // Целая часть
    int32_t nano;                // Дробная часть (10^-9)
    std::string currency;        // "RUB", "USD"
    
    double toDouble() const {
        return static_cast<double>(units) + static_cast<double>(nano) / 1e9;
    }
    
    static Money fromDouble(double value, const std::string& currency) {
        Money m;
        m.units = static_cast<int64_t>(value);
        m.nano = static_cast<int32_t>((value - m.units) * 1e9);
        m.currency = currency;
        return m;
    }
};

// Enums
enum class OrderDirection { BUY, SELL };
enum class OrderType { MARKET, LIMIT };
enum class OrderStatus { PENDING, FILLED, CANCELLED, REJECTED };
enum class StrategyStatus { STOPPED, RUNNING, ERROR };
enum class StrategyType { SMA_CROSSOVER };
enum class AccountType { SANDBOX, PRODUCTION };
enum class SignalType { BUY, SELL, HOLD };
```

---

# 5. Порты (Interfaces)

## 5.1 Input Ports (Use Cases)

### IAuthService
```cpp
class IAuthService {
public:
    virtual ~IAuthService() = default;
    
    // Логин и получение JWT
    virtual std::string login(const std::string& username) = 0;
    
    // Валидация JWT токена
    virtual bool validateToken(const std::string& token) = 0;
    
    // Получение userId из токена
    virtual std::string getUserIdFromToken(const std::string& token) = 0;
};
```

### IMarketService
```cpp
class IMarketService {
public:
    virtual ~IMarketService() = default;
    
    // Получить котировку по FIGI
    // Примечание: accountId НЕ нужен - цена на рынке одна для всех
    virtual Quote getQuote(const std::string& figi) = 0;
    
    // Получить несколько котировок
    virtual std::vector<Quote> getQuotes(const std::vector<std::string>& figis) = 0;
    
    // Поиск инструмента по тикеру
    virtual std::vector<Instrument> searchInstruments(const std::string& query) = 0;
};
```

### IOrderService
```cpp
class IOrderService {
public:
    virtual ~IOrderService() = default;
    
    // Создать ордер
    virtual Order placeOrder(const OrderRequest& request) = 0;
    
    // Отменить ордер
    virtual bool cancelOrder(const std::string& accountId, const std::string& orderId) = 0;
    
    // Получить список активных ордеров
    virtual std::vector<Order> getActiveOrders(const std::string& accountId) = 0;
    
    // Получить историю ордеров
    virtual std::vector<Order> getOrderHistory(
        const std::string& accountId,
        Timestamp from,
        Timestamp to
    ) = 0;
};
```

### IPortfolioService
```cpp
class IPortfolioService {
public:
    virtual ~IPortfolioService() = default;
    
    // Получить портфель
    virtual Portfolio getPortfolio(const std::string& accountId) = 0;
    
    // Получить позицию по инструменту
    virtual std::optional<Position> getPosition(
        const std::string& accountId, 
        const std::string& figi
    ) = 0;
};
```

### IStrategyService
```cpp
class IStrategyService {
public:
    virtual ~IStrategyService() = default;
    
    // Создать стратегию
    virtual Strategy createStrategy(const StrategyRequest& request) = 0;
    
    // Запустить стратегию
    virtual bool startStrategy(const std::string& strategyId) = 0;
    
    // Остановить стратегию
    virtual bool stopStrategy(const std::string& strategyId) = 0;
    
    // Получить статус стратегии
    virtual StrategyStatus getStatus(const std::string& strategyId) = 0;
    
    // Получить список стратегий пользователя
    virtual std::vector<Strategy> getUserStrategies(const std::string& accountId) = 0;
    
    // Получить последние сигналы
    virtual std::vector<Signal> getRecentSignals(const std::string& strategyId, int limit) = 0;
};
```

### IAccountService
```cpp
class IAccountService {
public:
    virtual ~IAccountService() = default;
    
    // Добавить счёт (сохранить токен)
    virtual Account addAccount(const std::string& userId, const AccountRequest& request) = 0;
    
    // Получить список счетов пользователя
    virtual std::vector<Account> getUserAccounts(const std::string& userId) = 0;
    
};
```

## 5.2 Output Ports (Repositories & Gateways)

### IBrokerGateway
```cpp
class IBrokerGateway {
public:
    virtual ~IBrokerGateway() = default;
    
    // Установить токен для запросов
    virtual void setAccessToken(const std::string& token) = 0;
    
    // Котировки
    virtual Quote getQuote(const std::string& figi) = 0;
    virtual std::vector<Quote> getQuotes(const std::vector<std::string>& figis) = 0;
    
    // Портфель
    virtual Portfolio getPortfolio() = 0;
    virtual std::vector<Position> getPositions() = 0;
    
    // Ордера
    virtual OrderResult placeOrder(const OrderRequest& order) = 0;
    virtual bool cancelOrder(const std::string& orderId) = 0;
    virtual std::vector<Order> getOrders() = 0;
    
    // Инструменты
    virtual std::vector<Instrument> searchInstruments(const std::string& query) = 0;
    virtual Instrument getInstrumentByFigi(const std::string& figi) = 0;
};
```

### IUserRepository
```cpp
class IUserRepository {
public:
    virtual ~IUserRepository() = default;
    
    virtual void save(const User& user) = 0;
    virtual std::optional<User> findById(const std::string& id) = 0;
    virtual std::optional<User> findByUsername(const std::string& username) = 0;
};
```

### IAccountRepository
```cpp
class IAccountRepository {
public:
    virtual ~IAccountRepository() = default;
    
    virtual void save(const Account& account) = 0;
    virtual std::optional<Account> findById(const std::string& id) = 0;
    virtual std::vector<Account> findByUserId(const std::string& userId) = 0;
    virtual void update(const Account& account) = 0;
};
```

### IOrderRepository
```cpp
class IOrderRepository {
public:
    virtual ~IOrderRepository() = default;
    
    virtual void save(const Order& order) = 0;
    virtual std::optional<Order> findById(const std::string& id) = 0;
    virtual std::vector<Order> findByAccountId(const std::string& accountId) = 0;
    virtual void updateStatus(const std::string& orderId, OrderStatus status) = 0;
};
```

### IStrategyRepository
```cpp
class IStrategyRepository {
public:
    virtual ~IStrategyRepository() = default;
    
    virtual void save(const Strategy& strategy) = 0;
    virtual std::optional<Strategy> findById(const std::string& id) = 0;
    virtual std::vector<Strategy> findByAccountId(const std::string& accountId) = 0;
    virtual void updateStatus(const std::string& id, StrategyStatus status) = 0;
};
```

### IJwtProvider
```cpp
class IJwtProvider {
public:
    virtual ~IJwtProvider() = default;
    
    // Создать токен
    virtual std::string createToken(const std::string& userId, const std::string& username) = 0;
    
    // Валидировать токен
    virtual bool validateToken(const std::string& token) = 0;
    
    // Извлечь claims
    virtual std::optional<JwtClaims> extractClaims(const std::string& token) = 0;
};
```

### ICache
```cpp
class ICache {
public:
    virtual ~ICache() = default;
    
    virtual void set(const std::string& key, const std::string& value, int ttlSeconds = 0) = 0;
    virtual std::optional<std::string> get(const std::string& key) = 0;
    virtual bool remove(const std::string& key) = 0;
    virtual bool exists(const std::string& key) = 0;
};
```

---

# 6. Адаптеры

## 6.1 Primary Adapters (REST Controllers)

| Controller | Endpoints | Порт (Input) |
|------------|-----------|--------------|
| `AuthController` | POST /auth/login | IAuthService |
| `AccountController` | GET/POST /accounts | IAccountService |
| `MarketController` | GET /quotes, GET /instruments | IMarketService |
| `OrderController` | POST/DELETE /orders | IOrderService |
| `PortfolioController` | GET /portfolio | IPortfolioService |
| `StrategyController` | GET/POST/PUT /strategies | IStrategyService |
| `HealthController` | GET /health | - |
| `MetricsController` | GET /metrics | - |

## 6.2 Secondary Adapters

### Persistence Adapters
| Adapter | Порт (Output) | Технология |
|---------|---------------|------------|
| `PostgresUserRepository` | IUserRepository | PostgreSQL |
| `PostgresAccountRepository` | IAccountRepository | PostgreSQL |
| `PostgresOrderRepository` | IOrderRepository | PostgreSQL |
| `PostgresStrategyRepository` | IStrategyRepository | PostgreSQL |

### External Service Adapters
| Adapter | Порт (Output) | Описание |
|---------|---------------|----------|
| `SimpleBrokerGatewayAdapter` | IBrokerGateway | **MVP**: Эмуляция Tinkoff API |
| `TinkoffGrpcAdapter` | IBrokerGateway | **Production**: Реальный gRPC |
| `FakeJwtAdapter` | IJwtProvider | Интеграция с fake-jwt-server |
| `LruCacheAdapter` | ICache | Обёртка над cpp-cache |
| `InMemoryEventBus` | IEventBus | **MVP**: События в памяти |
| `RabbitMqEventBus` | IEventBus | **Education**: RabbitMQ |

---

# 7. Event Bus

## 7.1 Порт IEventBus

```cpp
// Базовый класс события
struct DomainEvent {
    std::string eventId;
    std::string eventType;
    Timestamp timestamp;
    
    virtual ~DomainEvent() = default;
    virtual std::string toJson() const = 0;
};

// Callback для подписчиков
using EventHandler = std::function<void(const DomainEvent&)>;

class IEventBus {
public:
    virtual ~IEventBus() = default;
    
    // Публикация события
    virtual void publish(const DomainEvent& event) = 0;
    
    // Подписка на тип события
    virtual void subscribe(const std::string& eventType, EventHandler handler) = 0;
    
    // Отписка
    virtual void unsubscribe(const std::string& eventType) = 0;
};
```

## 7.2 Типы событий

| Событие | eventType | Описание |
|---------|-----------|----------|
| `OrderCreatedEvent` | order.created | Ордер создан |
| `OrderFilledEvent` | order.filled | Ордер исполнен |
| `OrderCancelledEvent` | order.cancelled | Ордер отменён |
| `StrategySignalEvent` | strategy.signal | Стратегия сгенерировала сигнал |
| `QuoteUpdatedEvent` | quote.updated | Котировка обновилась |

## 7.3 InMemoryEventBus (MVP)

Использует `ThreadSafeMap` из курса "Паттерны" для хранения подписчиков.

```cpp
class InMemoryEventBus : public IEventBus {
private:
    ThreadSafeMap<std::string, std::shared_ptr<std::vector<EventHandler>>> subscribers_;
    std::queue<std::shared_ptr<DomainEvent>> eventQueue_;
    std::mutex queueMutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{true};
    std::thread workerThread_;
    
public:
    void publish(const DomainEvent& event) override;
    void subscribe(const std::string& eventType, EventHandler handler) override;
    void unsubscribe(const std::string& eventType) override;
};
```

## 7.4 Переход на RabbitMQ (Education)

В Education создаём `RabbitMqEventBus` с тем же интерфейсом — меняем только DI конфигурацию.

---

# 8. Пользовательские сценарии MVP

## 8.1 US-001: Логин

```
POST /api/v1/auth/login
Content-Type: application/json

{
  "username": "trader1"
}

Response 200:
{
  "access_token": "eyJhbGciOiJIUzI1NiIs...",
  "token_type": "Bearer",
  "expires_in": 3600
}
```

## 8.2 US-002: Просмотр портфеля

```
GET /api/v1/portfolio
Authorization: Bearer <token>

Response 200:
{
  "totalValue": {"units": 1050000, "nano": 0, "currency": "RUB"},
  "cash": {"units": 200000, "nano": 0, "currency": "RUB"},
  "positions": [
    {
      "figi": "BBG004730N88",
      "ticker": "SBER",
      "quantity": 100,
      "averagePrice": {"units": 260, "nano": 0, "currency": "RUB"},
      "currentPrice": {"units": 265, "nano": 500000000, "currency": "RUB"},
      "pnl": {"units": 550, "nano": 0, "currency": "RUB"},
      "pnlPercent": 2.11
    }
  ]
}
```

## 8.3 US-003: Получение котировок

```
GET /api/v1/quotes?figis=BBG004730N88,BBG004731032
Authorization: Bearer <token>

Response 200:
{
  "quotes": [
    {
      "figi": "BBG004730N88",
      "ticker": "SBER",
      "lastPrice": {"units": 265, "nano": 500000000, "currency": "RUB"},
      "bidPrice": {"units": 265, "nano": 400000000, "currency": "RUB"},
      "askPrice": {"units": 265, "nano": 600000000, "currency": "RUB"},
      "updatedAt": "2025-12-16T10:30:00Z"
    }
  ]
}
```

## 8.4 US-004: Размещение ордера

```
POST /api/v1/orders
Authorization: Bearer <token>
Content-Type: application/json

{
  "figi": "BBG004730N88",
  "direction": "BUY",
  "type": "LIMIT",
  "quantity": 10,
  "price": {"units": 265, "nano": 0, "currency": "RUB"}
}

Response 201:
{
  "orderId": "order-123",
  "status": "PENDING",
  "createdAt": "2025-12-16T10:31:00Z"
}
```

## 8.5 US-005: Создание SMA стратегии

```
POST /api/v1/strategies
Authorization: Bearer <token>
Content-Type: application/json

{
  "name": "SBER SMA Crossover",
  "type": "SMA_CROSSOVER",
  "figi": "BBG004730N88",
  "config": {
    "shortPeriod": 10,
    "longPeriod": 30,
    "quantity": 10
  }
}

Response 201:
{
  "strategyId": "strategy-456",
  "status": "STOPPED",
  "createdAt": "2025-12-16T10:35:00Z"
}
```

## 8.6 US-006: Запуск стратегии

```
POST /api/v1/strategies/strategy-456/start
Authorization: Bearer <token>

Response 200:
{
  "strategyId": "strategy-456",
  "status": "RUNNING"
}
```

---

# 9. API спецификация

## 9.1 Полный список endpoints

| Method | Path | Описание | Auth |
|--------|------|----------|:----:|
| POST | `/api/v1/auth/login` | Получить JWT токен | ❌ |
| GET | `/api/v1/health` | Health check | ❌ |
| GET | `/metrics` | Prometheus metrics | ❌ |
| GET | `/api/v1/portfolio` | Получить портфель | ✅ |
| GET | `/api/v1/quotes` | Получить котировки | ✅ |
| GET | `/api/v1/instruments` | Поиск инструментов | ✅ |
| POST | `/api/v1/orders` | Создать ордер | ✅ |
| DELETE | `/api/v1/orders/{id}` | Отменить ордер | ✅ |
| GET | `/api/v1/orders` | Список ордеров | ✅ |
| POST | `/api/v1/strategies` | Создать стратегию | ✅ |
| GET | `/api/v1/strategies` | Список стратегий | ✅ |
| POST | `/api/v1/strategies/{id}/start` | Запустить стратегию | ✅ |
| POST | `/api/v1/strategies/{id}/stop` | Остановить стратегию | ✅ |
| GET | `/api/v1/strategies/{id}/signals` | Сигналы стратегии | ✅ |

## 9.2 Формат ошибок

```json
{
  "error": {
    "code": "INVALID_TOKEN",
    "message": "JWT token is invalid or expired",
    "timestamp": "2025-12-16T10:30:00Z"
  }
}
```

---

# 10. Схема данных

## 10.1 Справочные таблицы (Reference Tables)

```sql
-- ============================================
-- СПРАВОЧНИКИ
-- ============================================

CREATE TABLE ref_order_directions (
    code VARCHAR(10) PRIMARY KEY,
    name VARCHAR(50) NOT NULL
);

INSERT INTO ref_order_directions VALUES 
    ('BUY', 'Покупка'),
    ('SELL', 'Продажа');

CREATE TABLE ref_order_types (
    code VARCHAR(10) PRIMARY KEY,
    name VARCHAR(50) NOT NULL
);

INSERT INTO ref_order_types VALUES 
    ('MARKET', 'Рыночный'),
    ('LIMIT', 'Лимитный');

CREATE TABLE ref_order_statuses (
    code VARCHAR(20) PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    is_final BOOLEAN DEFAULT false
);

INSERT INTO ref_order_statuses VALUES 
    ('PENDING', 'Ожидает исполнения', false),
    ('FILLED', 'Исполнен', true),
    ('CANCELLED', 'Отменён', true),
    ('REJECTED', 'Отклонён', true);

CREATE TABLE ref_strategy_types (
    code VARCHAR(50) PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    description TEXT
);

INSERT INTO ref_strategy_types VALUES 
    ('SMA_CROSSOVER', 'SMA Crossover', 'Пересечение скользящих средних');

CREATE TABLE ref_strategy_statuses (
    code VARCHAR(20) PRIMARY KEY,
    name VARCHAR(50) NOT NULL
);

INSERT INTO ref_strategy_statuses VALUES 
    ('STOPPED', 'Остановлена'),
    ('RUNNING', 'Запущена'),
    ('ERROR', 'Ошибка');

CREATE TABLE ref_account_types (
    code VARCHAR(20) PRIMARY KEY,
    name VARCHAR(50) NOT NULL
);

INSERT INTO ref_account_types VALUES 
    ('SANDBOX', 'Песочница'),
    ('PRODUCTION', 'Боевой');

CREATE TABLE ref_signal_types (
    code VARCHAR(10) PRIMARY KEY,
    name VARCHAR(50) NOT NULL
);

INSERT INTO ref_signal_types VALUES 
    ('BUY', 'Покупать'),
    ('SELL', 'Продавать'),
    ('HOLD', 'Держать');
```

## 10.2 Основные таблицы

```sql
-- ============================================
-- ОСНОВНЫЕ ТАБЛИЦЫ
-- ============================================

CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    username VARCHAR(255) UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE accounts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id),
    name VARCHAR(255) NOT NULL,
    account_type VARCHAR(20) NOT NULL REFERENCES ref_account_types(code),
    access_token TEXT NOT NULL,
    is_active BOOLEAN DEFAULT false,
    created_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE orders (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    account_id UUID NOT NULL REFERENCES accounts(id),
    figi VARCHAR(50) NOT NULL,
    direction VARCHAR(10) NOT NULL REFERENCES ref_order_directions(code),
    order_type VARCHAR(10) NOT NULL REFERENCES ref_order_types(code),
    quantity BIGINT NOT NULL,
    price_units BIGINT,
    price_nano INT,
    price_currency VARCHAR(10),
    status VARCHAR(20) NOT NULL REFERENCES ref_order_statuses(code),
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE strategies (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    account_id UUID NOT NULL REFERENCES accounts(id),
    name VARCHAR(255) NOT NULL,
    strategy_type VARCHAR(50) NOT NULL REFERENCES ref_strategy_types(code),
    figi VARCHAR(50) NOT NULL,
    config JSONB NOT NULL,
    status VARCHAR(20) NOT NULL REFERENCES ref_strategy_statuses(code),
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE signals (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    strategy_id UUID NOT NULL REFERENCES strategies(id),
    signal_type VARCHAR(10) NOT NULL REFERENCES ref_signal_types(code),
    figi VARCHAR(50) NOT NULL,
    price_units BIGINT NOT NULL,
    price_nano INT NOT NULL,
    price_currency VARCHAR(10) NOT NULL,
    reason TEXT,
    created_at TIMESTAMP DEFAULT NOW()
);

-- ============================================
-- ИНДЕКСЫ
-- ============================================

CREATE INDEX idx_accounts_user_id ON accounts(user_id);
CREATE INDEX idx_orders_account_id ON orders(account_id);
CREATE INDEX idx_orders_status ON orders(status);
CREATE INDEX idx_strategies_account_id ON strategies(account_id);
CREATE INDEX idx_strategies_status ON strategies(status);
CREATE INDEX idx_signals_strategy_id ON signals(strategy_id);
CREATE INDEX idx_signals_created_at ON signals(created_at);
```

---

# 11. Технологический стек

## 11.1 Core Technologies

| Компонент | Технология | Источник |
|-----------|------------|----------|
| **Язык** | C++17 | - |
| **HTTP Server** | Boost.Beast + Boost.Asio | cpp-http-server lib |
| **DI Container** | Boost.DI | cpp-http-server lib |
| **Cache** | Custom LRU | cpp-cache lib |
| **JSON** | nlohmann/json | cpp-http-server lib |
| **Database** | PostgreSQL 15 | Docker |
| **ThreadSafeMap** | Custom | Курс "Паттерны" |

## 11.2 Infrastructure

| Компонент | Технология | Описание |
|-----------|------------|----------|
| **Auth** | fake-jwt-server | JWT токены для MVP |
| **Monitoring** | Prometheus | Сбор метрик |
| **Reverse Proxy** | Nginx | Статика + проксирование |
| **Containers** | Docker Compose | Оркестрация |
| **UI** | HTML + JS + Bootstrap | Простой веб-интерфейс |

## 11.3 Библиотеки (через FetchContent)

| Библиотека | Версия | Репозиторий |
|------------|--------|-------------|
| cpp-http-server | v0.0.5 | github.com/tobantal/cpp-http-server |
| cpp-cache | v0.0.1 | github.com/tobantal/cpp-cache |

## 11.4 Переиспользуемый код

| Курс OTUS | Что используем | Как |
|-----------|----------------|-----|
| Архитектура и паттерны | HTTP Server | FetchContent |
| Алгоритмы | LRU Cache | FetchContent |
| Паттерны проектирования | ThreadSafeMap, PostgreSQL utilities | Копируем в проект |

---

# 12. Структура проекта

```
mvp/
├── CMakeLists.txt
├── config.json
├── docker-compose.yml
├── Dockerfile
│
├── config/
│   ├── nginx.conf
│   └── prometheus.yml
│
├── migrations/
│   ├── 001_reference_tables.sql
│   └── 002_main_tables.sql
│
├── html/
│   ├── index.html
│   ├── css/styles.css
│   └── js/app.js
│
├── include/
│   │
│   ├── common/                         # Общие утилиты
│   │   └── ThreadSafeMap.hpp           # Из курса "Паттерны"
│   │
│   ├── domain/                         # ⭐ DOMAIN ENTITIES
│   │   ├── Account.hpp
│   │   ├── Order.hpp
│   │   ├── Position.hpp
│   │   ├── Quote.hpp
│   │   ├── Strategy.hpp
│   │   ├── Signal.hpp
│   │   ├── Money.hpp
│   │   └── Events.hpp                  # Domain Events
│   │
│   ├── ports/                          # ⭐ PORTS (INTERFACES)
│   │   ├── input/
│   │   │   ├── IAuthService.hpp
│   │   │   ├── IMarketService.hpp
│   │   │   ├── IOrderService.hpp
│   │   │   ├── IPortfolioService.hpp
│   │   │   ├── IStrategyService.hpp
│   │   │   └── IAccountService.hpp
│   │   │
│   │   └── output/
│   │       ├── IBrokerGateway.hpp
│   │       ├── IUserRepository.hpp
│   │       ├── IAccountRepository.hpp
│   │       ├── IOrderRepository.hpp
│   │       ├── IStrategyRepository.hpp
│   │       ├── IJwtProvider.hpp
│   │       ├── ICache.hpp
│   │       └── IEventBus.hpp
│   │
│   ├── application/                    # ⭐ APPLICATION SERVICES
│   │   ├── AuthService.hpp
│   │   ├── MarketService.hpp
│   │   ├── OrderService.hpp
│   │   ├── PortfolioService.hpp
│   │   ├── StrategyService.hpp
│   │   └── AccountService.hpp
│   │
│   ├── adapters/                       # ⭐ ADAPTERS
│   │   ├── primary/
│   │   │   ├── AuthController.hpp
│   │   │   ├── MarketController.hpp
│   │   │   ├── OrderController.hpp
│   │   │   ├── PortfolioController.hpp
│   │   │   ├── StrategyController.hpp
│   │   │   ├── HealthController.hpp
│   │   │   └── MetricsController.hpp
│   │   │
│   │   └── secondary/
│   │       ├── broker/
│   │       │   └── SimpleBrokerGatewayAdapter.hpp
│   │       ├── persistence/
│   │       │   ├── PostgresUserRepository.hpp
│   │       │   ├── PostgresAccountRepository.hpp
│   │       │   ├── PostgresOrderRepository.hpp
│   │       │   └── PostgresStrategyRepository.hpp
│   │       ├── auth/
│   │       │   └── FakeJwtAdapter.hpp
│   │       ├── cache/
│   │       │   └── LruCacheAdapter.hpp
│   │       └── events/
│   │           └── InMemoryEventBus.hpp
│   │
│   ├── strategies/                     # ⭐ TRADING ALGORITHMS
│   │   ├── IStrategy.hpp
│   │   ├── SmaStrategy.hpp
│   │   └── StrategyRunner.hpp
│   │
│   └── TradingApp.hpp
│
└── src/
    ├── main.cpp
    ├── TradingApp.cpp
    │
    ├── application/
    │   └── *.cpp
    │
    ├── adapters/
    │   ├── primary/
    │   │   └── *.cpp
    │   └── secondary/
    │       └── *.cpp
    │
    └── strategies/
        └── *.cpp
```

---

# 13. Критерии приёмки

## 13.1 Функциональные критерии MVP

| # | Сценарий | Приоритет | Статус |
|---|----------|:---------:|:------:|
| 1 | 🔐 Логин и получение JWT | P0 | ⬜ |
| 2 | 💼 Просмотр портфеля | P0 | ⬜ |
| 3 | 📊 Получение котировок | P0 | ⬜ |
| 4 | 💱 Создание ордера | P0 | ⬜ |
| 5 | ❌ Отмена ордера | P1 | ⬜ |
| 6 | 🤖 Создание SMA стратегии | P1 | ⬜ |
| 7 | ▶️ Запуск/остановка стратегии | P1 | ⬜ |
| 8 | 🌐 Web UI работает | P1 | ⬜ |

## 13.2 Архитектурные критерии

| # | Критерий | Статус |
|---|----------|:------:|
| 1 | 🏗️ Гексагональная архитектура (Ports & Adapters) | ⬜ |
| 2 | 📦 Структура проекта соответствует спецификации | ⬜ |
| 3 | 💉 DI через Boost.DI | ⬜ |
| 4 | 📤 REST API endpoints работают | ⬜ |
| 5 | 🐘 PostgreSQL с миграциями и справочниками | ⬜ |
| 6 | 📊 Prometheus метрики | ⬜ |
| 7 | 🐳 Docker Compose работает | ⬜ |
| 8 | 🧪 SimpleBrokerGatewayAdapter реализован | ⬜ |
| 9 | 🔐 fake-jwt-server интегрирован | ⬜ |
| 10 | 📨 InMemoryEventBus работает | ⬜ |

## 13.3 Команды для проверки

```bash
# Запуск всех сервисов
docker compose up -d

# Health check
curl http://localhost:8080/api/v1/health

# Логин
curl -X POST http://localhost:8080/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "trader1"}'

# Получить портфель (с токеном)
curl http://localhost:8080/api/v1/portfolio \
  -H "Authorization: Bearer <token>"

# Prometheus метрики
curl http://localhost:9090/metrics

# Web UI
open http://localhost:3001
```

---

## Приложения

### A. ThreadSafeMap (из курса "Паттерны")

```cpp
#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <memory>

template <typename K, typename V>
class ThreadSafeMap {
public:
    ThreadSafeMap() = default;

    void insert(const K& key, const std::shared_ptr<V>& value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        map_[key] = value;
    }

    std::shared_ptr<V> find(const K& key) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = map_.find(key);
        return (it != map_.end()) ? it->second : nullptr;
    }

    bool contains(const K& key) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return map_.find(key) != map_.end();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<K, std::shared_ptr<V>> map_;
};
```

### B. Глоссарий

| Термин | Описание |
|--------|---------|
| **FIGI** | Financial Instrument Global Identifier |
| **P&L** | Profit and Loss — прибыль/убыток |
| **SMA** | Simple Moving Average — простая скользящая средняя |
| **Lot** | Лот — минимальная единица торговли |
| **Sandbox** | Тестовая среда с виртуальными деньгами |
| **Port** | Интерфейс между доменом и адаптером |
| **Adapter** | Реализация интерфейса для конкретной технологии |

---

*Документ создан: 16 декабря 2025*  
*Версия: 2.1*
