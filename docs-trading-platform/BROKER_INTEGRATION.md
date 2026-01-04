# Интеграция Portfolio и Orders handlers в broker-service

## Проблема

Trading-service вызывает broker-service для получения portfolio и orders:
- `GET /api/v1/portfolio?account_id=...`
- `GET /api/v1/orders?account_id=...`

**Две причины 500 Internal Server Error:**

1. **Нет HTTP handlers** для `/api/v1/portfolio` и `/api/v1/orders`
2. **Несовпадение аккаунтов** - auth-service создаёт аккаунты динамически при регистрации, а FakeBrokerAdapter знает только о pre-seeded аккаунтах (`acc-001-sandbox`, etc.)

## Решение

Созданы файлы:

**HTTP Handlers:**
- `PortfolioHandler.hpp` - GET /api/v1/portfolio, /positions, /cash
- `OrdersHandler.hpp` - GET /api/v1/orders, /orders/{id}

**PostgreSQL репозитории (полноценные реализации):**
- `PostgresBrokerBalanceRepository.hpp` - балансы аккаунтов
- `PostgresBrokerPositionRepository.hpp` - позиции
- `PostgresBrokerOrderRepository.hpp` - ордера
- `PostgresQuoteRepository.hpp` - котировки
- `PostgresInstrumentRepository.hpp` - инструменты + seed data

**Обновлённый адаптер:**
- `FakeBrokerAdapter.hpp` - авто-регистрация + персистентность

## Шаги интеграции

### 1. Скопировать файлы

```bash
# HTTP Handlers
cp PortfolioHandler.hpp broker-service/include/adapters/primary/
cp OrdersHandler.hpp broker-service/include/adapters/primary/

# PostgreSQL репозитории
cp PostgresBrokerBalanceRepository.hpp broker-service/include/adapters/secondary/
cp PostgresBrokerPositionRepository.hpp broker-service/include/adapters/secondary/
cp PostgresBrokerOrderRepository.hpp broker-service/include/adapters/secondary/
cp PostgresQuoteRepository.hpp broker-service/include/adapters/secondary/
cp PostgresInstrumentRepository.hpp broker-service/include/adapters/secondary/

# Обновлённый адаптер
cp FakeBrokerAdapter.hpp broker-service/include/adapters/secondary/broker/
```

### 2. Обновить BrokerApp.hpp

**Добавить includes:**
```cpp
// В секции Primary Adapters добавить:
#include "adapters/primary/PortfolioHandler.hpp"
#include "adapters/primary/OrdersHandler.hpp"
```

**Добавить регистрацию handlers в `configureInjection()`:**

После регистрации QuotesHandler добавить:

```cpp
// Portfolio Handler
{
    auto handler = injector.create<std::shared_ptr<adapters::primary::PortfolioHandler>>();
    handlers_[getHandlerKey("GET", "/api/v1/portfolio")] = handler;
    handlers_[getHandlerKey("GET", "/api/v1/portfolio/positions")] = handler;
    handlers_[getHandlerKey("GET", "/api/v1/portfolio/cash")] = handler;
    std::cout << "  + PortfolioHandler: GET /api/v1/portfolio[/*]" << std::endl;
}

// Orders Handler (для GET запросов от trading-service)
{
    auto handler = injector.create<std::shared_ptr<adapters::primary::OrdersHandler>>();
    handlers_[getHandlerKey("GET", "/api/v1/orders")] = handler;
    handlers_[getHandlerKey("GET", "/api/v1/orders/*")] = handler;
    std::cout << "  + OrdersHandler: GET /api/v1/orders[/*]" << std::endl;
}
```

### 4. Обновить BrokerApp.hpp - DI для репозиториев

**В методе `configureInjection()` после создания injector добавить:**

```cpp
// Создаём FakeBrokerAdapter с репозиториями для персистентности
auto eventPublisher = injector.create<std::shared_ptr<ports::output::IEventPublisher>>();
auto balanceRepo = injector.create<std::shared_ptr<ports::output::IBrokerBalanceRepository>>();
auto positionRepo = injector.create<std::shared_ptr<ports::output::IBrokerPositionRepository>>();
auto orderRepo = injector.create<std::shared_ptr<ports::output::IBrokerOrderRepository>>();
auto quoteRepo = injector.create<std::shared_ptr<ports::output::IQuoteRepository>>();

auto brokerGateway = std::make_shared<adapters::secondary::FakeBrokerAdapter>(
    eventPublisher,
    balanceRepo,
    positionRepo,
    orderRepo,
    quoteRepo
);

// Сохраняем для использования в handlers
brokerGateway_ = brokerGateway;
```

### 3. Пересобрать и протестировать

```bash
cd build
cmake .. -DBUILD_TESTS=ON
make -j4 broker-service

# Локальный тест (если есть PostgreSQL)
./broker-service
```

## Персистентность данных

### Проблема
Все PostgreSQL репозитории были **заглушками** с пустыми методами:
```cpp
void save(const domain::BrokerBalance& balance) override {}  // ПУСТОЕ!
```

### Решение
Созданы полноценные реализации репозиториев:
- `PostgresBrokerBalanceRepository.hpp` - балансы аккаунтов
- `PostgresBrokerPositionRepository.hpp` - позиции по инструментам
- `PostgresQuoteRepository.hpp` - котировки
- `PostgresInstrumentRepository.hpp` - инструменты + seed data

### Автоматическая синхронизация
FakeBrokerAdapter теперь:
1. **При старте** - создаёт таблицы (CREATE TABLE IF NOT EXISTS)
2. **При регистрации аккаунта** - сохраняет баланс в БД
3. **При изменении цен** - сохраняет котировки в БД
4. **При создании ордера** - сохраняет ордер в БД
5. **При исполнении ордера** - обновляет ордер, баланс и позиции в БД
6. **При запросе ордеров** - читает из БД (полная история)

### Схема БД (создаётся автоматически)
```sql
-- Балансы
CREATE TABLE broker_balances (
    account_id VARCHAR(64) PRIMARY KEY,
    currency VARCHAR(3) NOT NULL DEFAULT 'RUB',
    available BIGINT NOT NULL DEFAULT 0,
    reserved BIGINT NOT NULL DEFAULT 0,
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Позиции
CREATE TABLE broker_positions (
    account_id VARCHAR(64) NOT NULL,
    figi VARCHAR(12) NOT NULL,
    quantity INTEGER NOT NULL,
    avg_price BIGINT NOT NULL,
    updated_at TIMESTAMP DEFAULT NOW(),
    PRIMARY KEY(account_id, figi)
);

-- Ордера
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

-- Инструменты
CREATE TABLE instruments (
    figi VARCHAR(12) PRIMARY KEY,
    ticker VARCHAR(12) NOT NULL,
    name VARCHAR(128) NOT NULL,
    currency VARCHAR(3) NOT NULL,
    lot_size INTEGER NOT NULL,
    min_price_increment BIGINT NOT NULL
);

-- Котировки
CREATE TABLE quotes (
    figi VARCHAR(12) PRIMARY KEY,
    bid BIGINT NOT NULL,
    ask BIGINT NOT NULL,
    last_price BIGINT NOT NULL,
    updated_at TIMESTAMP DEFAULT NOW()
);
```

### 4. Пересобрать Docker образ

```bash
docker build -t tobantal/broker-service:latest -f broker-service/Dockerfile .
docker push tobantal/broker-service:latest
```

### 5. Передеплоить в Kubernetes

```bash
kubectl rollout restart deployment/broker-service -n trading
kubectl logs -f deployment/broker-service -n trading
```

### 6. Проверить endpoints

```bash
# Portfolio
curl "http://arch.homework/broker/api/v1/portfolio?account_id=acc-sandbox-001"

# Positions
curl "http://arch.homework/broker/api/v1/portfolio/positions?account_id=acc-sandbox-001"

# Cash
curl "http://arch.homework/broker/api/v1/portfolio/cash?account_id=acc-sandbox-001"

# Orders
curl "http://arch.homework/broker/api/v1/orders?account_id=acc-sandbox-001"
```

### 7. Запустить Postman тесты trading-service

```bash
newman run trading-service.postman_collection.json
```

## Ожидаемые форматы ответов

### Portfolio
```json
{
  "account_id": "acc-sandbox-001",
  "cash": {"amount": 100000.0, "currency": "RUB"},
  "total_value": {"amount": 150000.0, "currency": "RUB"},
  "positions": [
    {
      "figi": "BBG004730N88",
      "ticker": "SBER",
      "quantity": 100,
      "average_price": 280.0,
      "current_price": 285.0,
      "currency": "RUB",
      "pnl": 500.0,
      "pnl_percent": 1.78
    }
  ]
}
```

### Positions
```json
{
  "account_id": "acc-sandbox-001",
  "positions": [...]
}
```

### Cash
```json
{
  "account_id": "acc-sandbox-001",
  "amount": 100000.0,
  "currency": "RUB"
}
```

### Orders
```json
{
  "orders": [
    {
      "id": "ord-123",
      "account_id": "acc-sandbox-001",
      "figi": "BBG004730N88",
      "direction": "BUY",
      "type": "MARKET",
      "quantity": 10,
      "price": 280.0,
      "currency": "RUB",
      "status": "FILLED",
      "created_at": "2025-01-03T12:00:00Z",
      "updated_at": "2025-01-03T12:00:01Z"
    }
  ]
}
```

## Seed данные

Убедитесь что в PostgreSQL есть seed данные для тестового аккаунта:

```sql
-- Баланс
INSERT INTO broker_balances (account_id, available, reserved, currency)
VALUES ('acc-sandbox-001', 10000000, 0, 'RUB')
ON CONFLICT (account_id) DO NOTHING;

-- Позиции (опционально)
INSERT INTO broker_positions (account_id, figi, quantity, average_price, currency)
VALUES ('acc-sandbox-001', 'BBG004730N88', 100, 28000, 'RUB')
ON CONFLICT (account_id, figi) DO NOTHING;
```

## Обновлённая структура handlers

```
broker-service/include/adapters/primary/
├── HealthHandler.hpp
├── MetricsHandler.hpp
├── InstrumentsHandler.hpp
├── QuotesHandler.hpp
├── PortfolioHandler.hpp   ← NEW
└── OrdersHandler.hpp      ← NEW
```
