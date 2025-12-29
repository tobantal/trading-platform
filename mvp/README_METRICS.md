# MVP Docker Package

## Структура

```
mvp/
├── Dockerfile
├── docker-compose.yml
├── config.json                 # парметры для DI
├── config/
│   └── prometheus.yml
├── sql/
│   └── init.sql
└── grafana/
    └── provisioning/
        ├── datasources/
        │   └── datasource.yml   # автоподключение к Prometheus
        └── dashboards/
            ├── dashboard.yml    # провайдер дашбордов
            └── trading-dashboard.json  # готовый дашборд
```


## Запуск

```bash
cd mvp/
docker compose up -d --build

# Проверка
docker compose ps
docker compose logs -f backend
```

## URL сервисов

| Сервис | URL | Логин |
|--------|-----|-------|
| Backend | http://localhost:8080 | - |
| Prometheus | http://localhost:9090 | - |
| Grafana | http://localhost:3000 | admin / admin |
| RabbitMQ | http://localhost:15672 | trading / trading123 |
| PostgreSQL | localhost:5432 | trader / password |

## Проверка Prometheus

1. Открой http://localhost:9090/targets
2. Target `trading-mvp` должен быть `UP`
3. Попробуй запрос: `trading_uptime_seconds`

## Grafana Dashboard

Автоматически загружается при старте:
1. Открой http://localhost:3000
2. Логин: admin / admin
3. Dashboards → Trading Platform MVP

## Дашборд включает:

- **Uptime** — время работы приложения
- **HTTP Requests** — всего запросов
- **Cache Hit Ratio** — эффективность кэша
- **Active Strategies** — запущенные стратегии
- **Orders by Status** — график created/filled/cancelled
- **Orders by Direction** — buy/sell
- **Strategy Signals** — сигналы стратегий
- **Cache Operations** — hits/misses
- **Order Fill Rate** — процент исполнения
- **Executed Volume** — объём в лотах
- **Errors Total** — ошибки

## Отладка

```bash
# Логи backend
docker compose logs backend

# Проверить метрики напрямую
curl http://localhost:8080/metrics

# Перезапустить
docker compose restart backend

# Полная перезборка
docker compose down
docker compose up -d --build
```

## Метрики которые теперь собираются:

### Application
- `trading_app_info` — информация о приложении
- `trading_uptime_seconds` — время работы

### HTTP
- `trading_http_requests_total` — всего HTTP запросов

### Auth
- `trading_auth_total{result="success|failure"}` — аутентификация

### Cache
- `trading_cache_operations_total{result="hit|miss"}` — кэш
- `trading_cache_hit_ratio` — процент попаданий

### Orders
- `trading_orders_total{status="created|filled|cancelled"}` — ордера по статусу
- `trading_orders_by_direction{direction="buy|sell"}` — по направлению
- `trading_orders_by_type{type="market|limit"}` — по типу
- `trading_executed_volume_total` — исполненный объём
- `trading_order_fill_rate` — процент исполнения

### Quotes
- `trading_quotes_received_total` — обновления котировок

### Strategies
- `trading_strategies_active` — активные стратегии
- `trading_strategies_total{action="started|stopped"}` — запуск/остановка
- `trading_strategy_signals_total{type="buy|sell"}` — сигналы

### Errors
- `trading_errors_total` — ошибки

---

## Пример вывода /metrics:

```prometheus
# HELP trading_app_info Application information
# TYPE trading_app_info gauge
trading_app_info{version="1.0.0",architecture="hexagonal",service="trading-mvp"} 1

# HELP trading_uptime_seconds Application uptime in seconds
# TYPE trading_uptime_seconds gauge
trading_uptime_seconds 3600

# HELP trading_orders_total Total orders by status
# TYPE trading_orders_total counter
trading_orders_total{status="created"} 150
trading_orders_total{status="filled"} 142
trading_orders_total{status="cancelled"} 8

# HELP trading_order_fill_rate Order fill rate
# TYPE trading_order_fill_rate gauge
trading_order_fill_rate 0.9466666666666667
```

---

## Grafana Dashboard (опционально)

После интеграции можно создать dashboard в Grafana:

1. Открыть http://localhost:3000 (admin/admin)
2. Add Data Source → Prometheus → http://prometheus:9090
3. Import Dashboard с метриками trading_*
