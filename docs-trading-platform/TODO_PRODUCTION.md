# TODO: Trading Platform — Production Roadmap

> **Дата:** 2025-01-07  
> **Статус:** Education ✅ завершён | Production 🔜  
> **Версия:** 1.0

---

## 📋 Содержание

1. [Критичные доработки библиотеки](#-p0-критичные-доработки-библиотеки-cpp-http-server)
2. [Архитектурные улучшения](#-p1-архитектурные-улучшения)
3. [Новый функционал](#-p2-новый-функционал)
4. [Технический долг](#-технический-долг)
5. [Рефакторинг](#-рефакторинг)

---

## 🔴 P0: Критичные доработки библиотеки cpp-http-server

### 3. Стандартизация обработки path

**Проблема:** Разные handlers по-разному парсят path, постоянные баги.

**Решение:** Добавить в Router встроенную поддержку path parameters:

```cpp
// Регистрация маршрута с параметрами
router.get("/api/v1/orders/:id", orderHandler);
router.delete("/api/v1/orders/:id", cancelHandler);

// В handler:
void handle(IRequest& req, IResponse& res) override {
    auto orderId = req.getPathParam("id");  // Автоматически извлечено
    // ...
}
```

---

## 🟡 P1: Архитектурные улучшения

### 4. Рефакторинг OrderProcessor

**Проблема:** Один класс обрабатывает все сценарии — сложно тестировать и поддерживать.

**Текущая структура:**
```cpp
class OrderProcessor {
    OrderResult processOrder(request, scenario) {
        switch (scenario.fillBehavior) {
            case IMMEDIATE: return fillImmediately(...);
            case REALISTIC: return processRealistic(...);  // if-else внутри
            case PARTIAL: return fillPartially(...);
            case DELAYED: return queueForDelayedFill(...);
            case ALWAYS_REJECT: return rejectOrder(...);
        }
    }
};
```

**Предлагаемая структура (Strategy Pattern):**
```cpp
// Базовый интерфейс
struct IOrderStrategy {
    virtual OrderResult process(const OrderRequest& req, const Quote& quote) = 0;
    virtual bool canHandle(const OrderRequest& req) const = 0;
};

// Конкретные стратегии
class MarketBuyStrategy : public IOrderStrategy { ... };
class MarketSellStrategy : public IOrderStrategy { ... };
class LimitBuyStrategy : public IOrderStrategy { ... };
class LimitSellStrategy : public IOrderStrategy { ... };

// Процессор выбирает стратегию
class OrderProcessor {
    std::vector<std::unique_ptr<IOrderStrategy>> strategies_;
    
    OrderResult processOrder(const OrderRequest& req, const MarketScenario& scenario) {
        for (auto& strategy : strategies_) {
            if (strategy->canHandle(req)) {
                return strategy->process(req, getQuote(req.figi));
            }
        }
        return reject("No suitable strategy");
    }
};
```

**См. ORDER_PROCESSOR.md** для детального анализа.

---

### 5. Circuit Breaker для inter-service communication

**Проблема:** При падении broker-service, trading-service продолжает слать запросы.

**Решение:**
```cpp
class CircuitBreaker {
    enum State { CLOSED, OPEN, HALF_OPEN };
    
    template<typename F>
    auto execute(F&& func) {
        if (state_ == OPEN && !shouldAttemptReset()) {
            throw CircuitOpenException();
        }
        try {
            auto result = func();
            onSuccess();
            return result;
        } catch (...) {
            onFailure();
            throw;
        }
    }
};
```

---

### 6. Distributed Tracing (X-Trace-ID)

**Проблема:** Сложно отследить запрос через все сервисы.

**Решение:**
- Генерировать `X-Trace-ID` в Ingress (или первом сервисе)
- Пробрасывать через все HTTP и RabbitMQ вызовы
- Логировать с trace_id

```cpp
// В каждом handler:
auto traceId = req.getHeader("X-Trace-ID").value_or(generateTraceId());
logger.info("[{}] Processing order {}", traceId, orderId);
```

---

## 🟢 P2: Новый функционал

### 7. Реальный Tinkoff Invest API

| Задача | Оценка | Описание |
|--------|--------|----------|
| gRPC клиент | 8h | Подключение к Tinkoff API |
| TinkoffBrokerAdapter | 4h | Реализация IBrokerGateway |
| Streaming котировок | 4h | WebSocket/gRPC streaming |
| Обработка ошибок API | 2h | Retry, rate limiting |

---

### 8. Trading UI (React)

| Задача | Оценка | Описание |
|--------|--------|----------|
| React + Vite setup | 2h | Базовый проект |
| Auth flow | 4h | Login, Register, Token refresh |
| Dashboard | 4h | Portfolio, P&L |
| Order form | 2h | Market/Limit orders |
| Order history | 2h | Список ордеров |
| Real-time updates | 4h | WebSocket для котировок |

---

### 9. Торговые стратегии

| Стратегия | Описание |
|-----------|----------|
| SMA Crossover | Пересечение скользящих средних |
| RSI | Relative Strength Index |
| MACD | Moving Average Convergence Divergence |
| Mean Reversion | Возврат к среднему |

---

### 10. Backtesting

```cpp
class Backtester {
    BacktestResult run(
        const Strategy& strategy,
        const HistoricalData& data,
        const BacktestConfig& config
    );
};

struct BacktestResult {
    double totalReturn;
    double sharpeRatio;
    double maxDrawdown;
    std::vector<Trade> trades;
};
```

---

## 📦 Технический долг

| Долг | Приоритет | Оценка | Описание |
|------|-----------|--------|----------|
| `getPath()` контракт | P0 | 2h | Документировать и исправить везде |
| Типизация событий | P1 | 4h | Все события через DomainEvent классы |
| PortfolioUpdatedEvent | P1 | 1h | Сейчас формируется как raw JSON |
| InMemoryEventBus | P2 | 2h | Для unit-тестов без RabbitMQ |
| Bcrypt для паролей | P2 | 2h | Заменить простой hash |
| Rate limiting | P2 | 4h | Защита от DDoS |
| Request validation | P1 | 4h | Централизованная валидация JSON |

---

## 🔧 Рефакторинг

### Handler → Controller организация

**Текущее:** Один handler = один endpoint, много файлов.

**Предложение:**
```cpp
// Controller группирует связанные endpoints
class OrderController {
public:
    void createOrder(IRequest& req, IResponse& res);
    void getOrder(IRequest& req, IResponse& res);
    void cancelOrder(IRequest& req, IResponse& res);
    void listOrders(IRequest& req, IResponse& res);
};

// Router регистрирует методы контроллера
router.post("/api/v1/orders", &OrderController::createOrder);
router.get("/api/v1/orders/:id", &OrderController::getOrder);
router.delete("/api/v1/orders/:id", &OrderController::cancelOrder);
router.get("/api/v1/orders", &OrderController::listOrders);
```

---

### Разделение hpp/cpp

**Текущее:** Много логики в header файлах.

**Предложение:**
- hpp: только интерфейсы, шаблоны, inline функции
- cpp: вся имплементация
- Ускорит компиляцию, улучшит читаемость

---

## 📊 Приоритеты

```
┌─────────────────────────────────────────────────────────────────┐
│                    PRODUCTION ROADMAP                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Phase 1 (2 недели): Библиотека + Рефакторинг                   │
│  ├── IRequest/IResponse расширение                              │
│  ├── Path routing стандартизация                                │
│  └── OrderProcessor refactoring                                 │
│                                                                 │
│  Phase 2 (2 недели): Надёжность                                 │
│  ├── Circuit Breaker                                            │
│  ├── Distributed Tracing                                        │
│  └── Rate limiting                                              │
│                                                                 │
│  Phase 3 (4 недели): Tinkoff Integration                        │
│  ├── gRPC клиент                                                │
│  ├── TinkoffBrokerAdapter                                       │
│  └── Streaming котировок                                        │
│                                                                 │
│  Phase 4 (2 недели): UI                                         │
│  ├── React dashboard                                            │
│  └── Real-time updates                                          │
│                                                                 │
│  Phase 5 (ongoing): Стратегии + Backtesting                     │
│  └── SMA, RSI, MACD, Backtester                                 │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

*Последнее обновление: 2026-01-19*
