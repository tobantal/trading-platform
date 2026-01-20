# Архитектура метрик (Prometheus)

> **Статус:** Согласовано  
> **Дата:** 2025-01-05

---

## Обзор

Система сбора метрик для Trading Platform с интеграцией Prometheus/Grafana.

**Принципы:**
- Не трогаем рабочий код — добавляем "рядом"
- Декоратор для HTTP метрик
- Event Listener для бизнес-метрик (только Trading Service)
- ShardedCache для хранения счётчиков (потокобезопасный, без mutex)
- Только counter метрики

---

## Схема компонентов

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         TRADING SERVICE                                 │
└─────────────────────────────────────────────────────────────────────────┘

              HTTP Request
                   │
                   ▼
        ┌─────────────────────┐
        │MetricsDecoratorHandler│──────┐
        │   (wraps handlers)    │      │
        └──────────┬────────────┘      │
                   │                   │ increment()
                   ▼                   │
        ┌─────────────────────┐        │
        │   OrderHandler      │        │
        │   PortfolioHandler  │        │
        │   MarketHandler     │        │
        └─────────────────────┘        │
                                       ▼
                              ┌─────────────────┐
          RabbitMQ            │                 │        GET /metrics
              │               │  MetricsService │◀────────────────────┐
              ▼               │                 │                     │
        ┌───────────────┐     │  ShardedCache   │        ┌────────────┴──┐
        │AllEventsListener│───▶│  <string,int64>│◀───────│MetricsHandler │
        │               │     │                 │        └───────────────┘
        │ order.created │     └─────────────────┘
        │ order.filled  │            │
        │ order.rejected│            │ toPrometheusFormat()
        │ portfolio.*   │            ▼
        └───────────────┘     ┌─────────────────┐
              increment()     │   Prometheus    │
                              │   (scrapes)     │
                              └─────────────────┘
```

---

## Интерфейсы и классы

### MetricDefinition

```cpp
struct MetricDefinition {
    std::string name;   // "http_requests_total"
    std::string help;   // "Total HTTP requests"
    std::string type;   // "counter"
};
```

---

### IMetricsSettings

```cpp
class IMetricsSettings {
public:
    virtual ~IMetricsSettings() = default;
    
    virtual std::vector<MetricDefinition> getDefinitions() const = 0;
    virtual std::vector<std::string> getAllKeys() const = 0;
};
```

---

### MetricsSettings (Trading Service)

```cpp
class MetricsSettings : public IMetricsSettings {
public:
    std::vector<MetricDefinition> getDefinitions() const override {
        return {
            {"http_requests_total", "Total HTTP requests", "counter"},
            {"events_received_total", "Total events received", "counter"},
            {"orders_created_total", "Total orders created", "counter"},
            {"orders_filled_total", "Total orders filled", "counter"},
            {"orders_rejected_total", "Total orders rejected", "counter"},
            {"orders_cancelled_total", "Total orders cancelled", "counter"}
        };
    }
    
    std::vector<std::string> getAllKeys() const override {
        return {
            // HTTP метрики (method + path)
            "http_requests_total{method=\"GET\",path=\"/health\"}",
            "http_requests_total{method=\"GET\",path=\"/metrics\"}",
            "http_requests_total{method=\"GET\",path=\"/api/v1/portfolio\"}",
            "http_requests_total{method=\"GET\",path=\"/api/v1/orders\"}",
            "http_requests_total{method=\"POST\",path=\"/api/v1/orders\"}",
            "http_requests_total{method=\"DELETE\",path=\"/api/v1/orders\"}",
            "http_requests_total{method=\"GET\",path=\"/api/v1/instruments\"}",
            "http_requests_total{method=\"GET\",path=\"/api/v1/quotes\"}",
            
            // Event метрики
            "events_received_total{event=\"order.created\"}",
            "events_received_total{event=\"order.filled\"}",
            "events_received_total{event=\"order.rejected\"}",
            "events_received_total{event=\"order.cancelled\"}",
            "events_received_total{event=\"portfolio.updated\"}",
            
            // Бизнес метрики
            "orders_created_total",
            "orders_filled_total",
            "orders_rejected_total",
            "orders_cancelled_total"
        };
    }
};
```

---

### IMetricsService

```cpp
class IMetricsService {
public:
    virtual ~IMetricsService() = default;
    
    virtual void increment(
        const std::string& name, 
        const std::map<std::string, std::string>& labels = {}
    ) = 0;
    
    virtual std::string toPrometheusFormat() const = 0;
};
```

---

### MetricsService

```cpp
#include <cache/concurrency/ShardedCache.hpp>
#include <cache/Cache.hpp>
#include <cache/eviction/LRUPolicy.hpp>

class MetricsService : public IMetricsService {
public:
    explicit MetricsService(std::shared_ptr<IMetricsSettings> settings)
        : settings_(std::move(settings))
    {
        size_t capacity = settings_->getAllKeys().size();
        
        counters_ = std::make_unique<ShardedCache<std::string, int64_t, 16>>(
            capacity, 
            [](size_t cap) {
                return std::make_unique<Cache<std::string, int64_t>>(
                    cap, 
                    std::make_unique<LRUPolicy<std::string>>()
                );
            }
        );
        
        // Инициализируем все метрики нулями
        for (const auto& key : settings_->getAllKeys()) {
            counters_->put(key, 0);
        }
    }
    
    void increment(
        const std::string& name, 
        const std::map<std::string, std::string>& labels = {}
    ) override {
        std::string key = buildKey(name, labels);
        
        auto current = counters_->get(key);
        int64_t newValue = current ? (*current + 1) : 1;
        counters_->put(key, newValue);
    }
    
    std::string toPrometheusFormat() const override {
        std::ostringstream oss;
        
        // HELP и TYPE
        for (const auto& def : settings_->getDefinitions()) {
            oss << "# HELP " << def.name << " " << def.help << "\n";
            oss << "# TYPE " << def.name << " " << def.type << "\n";
        }
        oss << "\n";
        
        // Значения — итерируем по известным ключам из settings
        for (const auto& key : settings_->getAllKeys()) {
            auto value = counters_->get(key);
            if (value) {
                oss << key << " " << *value << "\n";
            }
        }
        
        return oss.str();
    }

private:
    std::shared_ptr<IMetricsSettings> settings_;
    std::unique_ptr<ShardedCache<std::string, int64_t, 16>> counters_;
    
    std::string buildKey(
        const std::string& name, 
        const std::map<std::string, std::string>& labels
    ) const {
        if (labels.empty()) {
            return name;
        }
        
        std::ostringstream oss;
        oss << name << "{";
        bool first = true;
        for (const auto& [k, v] : labels) {
            if (!first) oss << ",";
            oss << k << "=\"" << v << "\"";
            first = false;
        }
        oss << "}";
        return oss.str();
    }
};
```

---

### MetricsHandler

```cpp
class MetricsHandler : public IHttpHandler {
public:
    explicit MetricsHandler(std::shared_ptr<IMetricsService> metrics)
        : metrics_(std::move(metrics))
    {}
    
    void handle(IRequest& req, IResponse& res) override {
        res.setStatus(200);
        res.setHeader("Content-Type", "text/plain; version=0.0.4; charset=utf-8");
        res.setBody(metrics_->toPrometheusFormat());
    }

private:
    std::shared_ptr<IMetricsService> metrics_;
};
```

---

### MetricsDecoratorHandler

```cpp
class MetricsDecoratorHandler : public IHttpHandler {
public:
    MetricsDecoratorHandler(
        std::shared_ptr<IHttpHandler> inner,
        std::shared_ptr<IMetricsService> metrics
    ) : inner_(std::move(inner))
      , metrics_(std::move(metrics))
    {}
    
    void handle(IRequest& req, IResponse& res) override {
        // Инкрементируем ДО обработки (считаем входящие запросы)
        metrics_->increment("http_requests_total", {
            {"method", req.getMethod()},
            {"path", req.getPath()}
        });
        
        // Делегируем обработку
        inner_->handle(req, res);
    }

private:
    std::shared_ptr<IHttpHandler> inner_;
    std::shared_ptr<IMetricsService> metrics_;
};
```

---

### AllEventsListener

```cpp
class AllEventsListener {
public:
    AllEventsListener(
        std::shared_ptr<ports::input::IEventConsumer> consumer,
        std::shared_ptr<IMetricsService> metrics
    ) : consumer_(std::move(consumer))
      , metrics_(std::move(metrics))
    {
        consumer_->subscribe(
            {"order.created", "order.filled", "order.rejected", 
             "order.cancelled", "portfolio.updated"},
            [this](const std::string& routingKey, const std::string& message) {
                onEvent(routingKey, message);
            }
        );
    }
    
    void start() { consumer_->start(); }
    void stop() { consumer_->stop(); }
    
private:
    std::shared_ptr<ports::input::IEventConsumer> consumer_;
    std::shared_ptr<IMetricsService> metrics_;
    
    void onEvent(const std::string& routingKey, const std::string& message) {
        metrics_->increment("events_received_total", {{"event", routingKey}});
        
        if (routingKey == "order.created") {
            metrics_->increment("orders_created_total");
        } else if (routingKey == "order.filled") {
            metrics_->increment("orders_filled_total");
        } else if (routingKey == "order.rejected") {
            metrics_->increment("orders_rejected_total");
        } else if (routingKey == "order.cancelled") {
            metrics_->increment("orders_cancelled_total");
        }
    }
};
```

---

## Использование в DI (TradingApp)

```cpp
// 1. Создаём MetricsSettings и MetricsService
auto metricsSettings = std::make_shared<MetricsSettings>();
auto metricsService = std::make_shared<MetricsService>(metricsSettings);

// 2. MetricsHandler для /metrics
auto metricsHandler = std::make_shared<MetricsHandler>(metricsService);
handlers_[getHandlerKey("GET", "/metrics")] = metricsHandler;

// 3. Оборачиваем нужные handlers в MetricsDecoratorHandler
auto orderHandler = injector.create<std::shared_ptr<OrderHandler>>();
auto wrappedOrderHandler = std::make_shared<MetricsDecoratorHandler>(
    orderHandler, metricsService
);
handlers_[getHandlerKey("POST", "/api/v1/orders")] = wrappedOrderHandler;

// 4. AllEventsListener (отдельный consumer)
auto metricsConsumer = std::make_shared<RabbitMQAdapter>(...);  // отдельный consumer
auto eventsListener = std::make_shared<AllEventsListener>(metricsConsumer, metricsService);
eventsListener->start();
```

---

## Структура файлов (Trading Service)

```
trading-service/
├── include/
│   ├── ports/
│   │   └── input/
│   │       └── IMetricsService.hpp
│   ├── settings/
│   │   ├── IMetricsSettings.hpp
│   │   └── MetricsSettings.hpp
│   ├── application/
│   │   └── MetricsService.hpp
│   └── adapters/
│       └── primary/
│           ├── MetricsHandler.hpp
│           ├── MetricsDecoratorHandler.hpp
│           └── AllEventsListener.hpp
```

---

## Применение к другим сервисам

Auth Service и Broker Service используют тот же паттерн, но:
- **Без AllEventsListener** (только Trading слушает все события)
- **Свои MetricsSettings** с соответствующими endpoints

---

## План реализации

| # | Шаг | Описание |
|---|-----|----------|
| 1 | Интерфейсы | IMetricsSettings, IMetricsService |
| 2 | MetricsSettings | Конфиг для Trading Service |
| 3 | MetricsService | Реализация с ShardedCache |
| 4 | MetricsHandler | Endpoint GET /metrics |
| 5 | MetricsDecoratorHandler | Обёртка для HTTP handlers |
| 6 | AllEventsListener | Слушатель RabbitMQ событий |
| 7 | DI интеграция | TradingApp.hpp |
| 8 | k8s | Prometheus + Grafana manifests |
| 9 | E2E тест | Проверка /metrics |
| 10 | README | Документация |

---

## Ограничения текущей реализации

1. **Без HTTP status** — IResponse не имеет getStatus()
2. **Только counters** — без histograms/gauges
3. **Фиксированные labels** — все ключи определяются в settings заранее
4. **ShardedCache<..., 16>** — захардкожено 16 шардов

---

## Prometheus формат (пример вывода)

```
# HELP http_requests_total Total HTTP requests
# TYPE http_requests_total counter
# HELP events_received_total Total events received
# TYPE events_received_total counter
# HELP orders_created_total Total orders created
# TYPE orders_created_total counter
# HELP orders_filled_total Total orders filled
# TYPE orders_filled_total counter
# HELP orders_rejected_total Total orders rejected
# TYPE orders_rejected_total counter
# HELP orders_cancelled_total Total orders cancelled
# TYPE orders_cancelled_total counter

http_requests_total{method="GET",path="/health"} 42
http_requests_total{method="GET",path="/metrics"} 10
http_requests_total{method="POST",path="/api/v1/orders"} 15
events_received_total{event="order.created"} 15
events_received_total{event="order.filled"} 12
orders_created_total 15
orders_filled_total 12
orders_rejected_total 3
```
