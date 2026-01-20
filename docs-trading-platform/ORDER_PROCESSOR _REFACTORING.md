# OrderProcessor — Рефакторинг FakeBroker

> **Файл:** `broker-service/include/adapters/secondary/broker/OrderProcessor.hpp`  
> **Статус:** Технический долг  
> **Приоритет:** Низкий (после интеграции с Tinkoff)  
> **Оценка:** 4-6 часов

---

## 📋 Содержание

1. [Текущее состояние](#текущее-состояние)
2. [Проблемы](#проблемы)
3. [Решение: 4 независимых процессора](#решение-4-независимых-процессора)
4. [Реализация](#реализация)
5. [Связанные документы](#связанные-документы)

---

## Текущее состояние

### Роль OrderProcessor

OrderProcessor — часть FakeBrokerAdapter, симулирует поведение биржи для тестов и демонстрации.

```
OrderCommandHandler
       │
       ▼
IBrokerGateway (порт)
       │
       ▼
FakeBrokerAdapter ◄── для тестов/демо
       │
       ▼
EnhancedFakeBroker
       │
       ▼
OrderProcessor ◄── ЭТОТ КЛАСС
```

### Сценарии симуляции

| Инструмент | Сценарий | Поведение |
|------------|----------|-----------|
| SBER | immediate | Сразу FILLED |
| GAZP | alwaysReject | Всегда REJECTED |
| LKOH | delayed(5s) | PENDING → через 5s FILLED |
| MGNT | partialFill(50%) | 50% количества |
| YNDX | realistic | MARKET=сразу, LIMIT=ждёт цену |

---

## Проблемы

### 1. Божественный класс (~400 строк)

```cpp
class OrderProcessor {
    processOrder()           // точка входа
    fillImmediately()        // IMMEDIATE
    processRealistic()       // REALISTIC с if-else для MARKET/LIMIT
    fillPartially()          // PARTIAL
    queueForDelayedFill()    // DELAYED
    queueLimitOrder()        // отдельно для LIMIT
    processPendingOrders()   // background обработка
    cancelOrder()            // отмена
};
```

### 2. Много вложенных if-else

```cpp
OrderResult processOrder(...) {
    if (scenario.fillBehavior == IMMEDIATE) {
        // ...
    } else if (scenario.fillBehavior == REALISTIC) {
        if (request.type == MARKET) {
            // ...
        } else { // LIMIT
            if (request.direction == BUY) {
                // ...
            } else { // SELL
                // ...
            }
        }
    } else if (...) {
        // ...
    }
}
```

### 3. Дублирование логики проверки цены LIMIT

Проверка "можно ли исполнить LIMIT" повторяется в 3 местах:
- processRealistic()
- queueForDelayedFill()
- processPendingOrders()

### 4. Сложность тестирования

Для теста одного сценария нужен весь контекст: OrderProcessor + PriceSimulator + MarketScenario.

---

## Решение: 4 независимых процессора

### Идея

Разбить по Direction + Type:

```
OrderProcessor (400 строк)     →    4 процессора (~80-100 строк каждый)
                                           │
                                           ▼
                                   ┌─────────────────┐
                                   │ MarketBuyProc   │
                                   ├─────────────────┤
                                   │ MarketSellProc  │
                                   ├─────────────────┤
                                   │ LimitBuyProc    │
                                   ├─────────────────┤
                                   │ LimitSellProc   │
                                   └─────────────────┘
```

### Преимущества

| Аспект | 1 класс (сейчас) | 4 класса (будет) |
|--------|------------------|------------------|
| Строк кода | ~400 | ~80-100 каждый |
| Понимание | 30+ минут | 5 минут на класс |
| Тестирование | Сложно изолировать | Полная изоляция |
| Изменения | Риск сломать всё | Независимые изменения |

### Дублирование — сознательный выбор

Да, будет дублирование между процессорами. Но:
- Каждый процессор **полностью независим**
- Можно менять логику одного без влияния на другие
- Проще понять и отладить
- **Связанность** — большее зло, чем дублирование

---

## Реализация

### Интерфейс

```cpp
struct IOrderProcessor {
    virtual ~IOrderProcessor() = default;
    
    virtual OrderResult process(
        const OrderRequest& request,
        const Quote& quote,
        const MarketScenario& scenario
    ) = 0;
};
```

### MarketBuyProcessor

```cpp
class MarketBuyProcessor : public IOrderProcessor {
public:
    explicit MarketBuyProcessor(std::shared_ptr<PendingOrderQueue> pendingQueue);
    
    OrderResult process(
        const OrderRequest& request,
        const Quote& quote,
        const MarketScenario& scenario
    ) override {
        switch (scenario.fillBehavior) {
            case IMMEDIATE:
                return fillAt(request, quote.ask);
                
            case REALISTIC:
                return fillWithSlippage(request, quote.ask, scenario);
                
            case DELAYED:
                return queueDelayed(request, scenario.fillDelay);
                
            case PARTIAL:
                return fillPartially(request, quote.ask, scenario.partialFillRatio);
                
            case ALWAYS_REJECT:
                return reject(request, scenario.rejectReason);
        }
    }
    
private:
    OrderResult fillAt(const OrderRequest& req, double price);
    OrderResult fillWithSlippage(const OrderRequest& req, double basePrice, const MarketScenario& s);
    OrderResult queueDelayed(const OrderRequest& req, std::chrono::milliseconds delay);
    OrderResult fillPartially(const OrderRequest& req, double price, double ratio);
    OrderResult reject(const OrderRequest& req, const std::string& reason);
};
```

### LimitBuyProcessor

```cpp
class LimitBuyProcessor : public IOrderProcessor {
public:
    explicit LimitBuyProcessor(std::shared_ptr<PendingOrderQueue> pendingQueue);
    
    OrderResult process(
        const OrderRequest& request,
        const Quote& quote,
        const MarketScenario& scenario
    ) override {
        // LIMIT BUY: исполняем если ask <= limitPrice
        if (quote.ask <= request.price) {
            return fillAt(request, quote.ask);
        }
        
        // Иначе в очередь
        pendingQueue_->addLimitOrder(request);
        return pending(request, "Waiting for ask <= " + std::to_string(request.price));
    }
    
private:
    OrderResult fillAt(const OrderRequest& req, double price);
    OrderResult pending(const OrderRequest& req, const std::string& message);
};
```

### PendingOrderQueue (общий)

```cpp
class PendingOrderQueue {
public:
    void addDelayedMarket(const OrderRequest& req, std::chrono::milliseconds delay);
    void addLimitOrder(const OrderRequest& req);
    bool cancel(const std::string& orderId);
    
    std::vector<PendingOrder> getReadyToFill(
        std::chrono::system_clock::time_point now,
        const std::function<std::optional<Quote>(const std::string&)>& getQuote
    );
    
    void remove(const std::string& orderId);
    
private:
    std::mutex mutex_;
    std::unordered_map<std::string, PendingOrder> orders_;
};
```

### OrderProcessorFacade

```cpp
class OrderProcessorFacade {
public:
    OrderProcessorFacade(
        std::unique_ptr<IOrderProcessor> marketBuy,
        std::unique_ptr<IOrderProcessor> marketSell,
        std::unique_ptr<IOrderProcessor> limitBuy,
        std::unique_ptr<IOrderProcessor> limitSell,
        std::shared_ptr<PriceSimulator> priceSimulator
    );
    
    OrderResult processOrder(const OrderRequest& request, const MarketScenario& scenario) {
        if (shouldReject(scenario)) {
            return reject(request, scenario.rejectReason);
        }
        
        auto quote = priceSimulator_->getQuote(request.figi);
        if (!quote) {
            return reject(request, "Instrument not found");
        }
        
        auto* processor = selectProcessor(request);
        return processor->process(request, *quote, scenario);
    }
    
private:
    IOrderProcessor* selectProcessor(const OrderRequest& req) {
        if (req.type == MARKET) {
            return (req.direction == BUY) ? marketBuy_.get() : marketSell_.get();
        } else {
            return (req.direction == BUY) ? limitBuy_.get() : limitSell_.get();
        }
    }
};
```

### Структура файлов после рефакторинга

```
broker-service/include/adapters/secondary/broker/
├── processors/
│   ├── IOrderProcessor.hpp
│   ├── MarketBuyProcessor.hpp
│   ├── MarketSellProcessor.hpp
│   ├── LimitBuyProcessor.hpp
│   ├── LimitSellProcessor.hpp
│   └── OrderProcessorFacade.hpp
├── PendingOrderQueue.hpp
├── PriceSimulator.hpp
└── EnhancedFakeBroker.hpp  (использует OrderProcessorFacade)
```

---

## Связанные документы

- **[TINKOFF_INTEGRATION.md](./TINKOFF_INTEGRATION.md)** — интеграция с реальным брокером Tinkoff Invest API
- **[TODO_PRODUCTION.md](./TODO_PRODUCTION.md)** — общий production roadmap

---

*Документ создан: 2025-01-07*
