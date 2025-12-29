# ROADMAP v2: MVP → Микросервисы

> Дата: 2025-12-28  
> Статус: Утверждённый план

---

## Архитектура

```
                            ┌─────────────────┐
                            │   API Gateway   │
                            │  (nginx :3001)  │
                            └────────┬────────┘
                                     │
         ┌───────────────────────────┼───────────────────────────┐
         │                           │                           │
         ▼                           ▼                           ▼
┌─────────────────┐        ┌─────────────────┐        ┌─────────────────┐
│  Auth Service   │◀─REST──│ Trading Service │──REST──▶│ Broker Service  │
│     :8081       │        │     :8082       │        │  (Fake Tinkoff) │
│                 │        │                 │        │     :8083       │
│ • /auth/*       │        │ • /orders/*     │        │                 │
│ • /accounts/*   │        │ • /portfolio/*  │        │ • /quotes/*     │
│                 │        │ • /quotes/*     │        │ • /instruments/*│
└────────┬────────┘        └────────┬────────┘        │ • /broker/*     │
         │                          │                 └─────────────────┘
         │                          │
         │                          │ publish/subscribe
         │                          ▼
         │                 ┌─────────────────┐
         │                 │    RabbitMQ     │
         │                 │     :5672       │
         │                 │                 │
         │                 │ Exchanges:      │
         │                 │ • orders        │
         │                 │ • quotes        │
         │                 └─────────────────┘
         │
         ▼
┌─────────────────┐
│   PostgreSQL    │  (в конце проекта)
│     :5432       │
│                 │
│ • auth_db       │
│ • trading_db    │
│ • broker_db     │
└─────────────────┘
```

---

## Решения по архитектуре

### Микросервисы (3 штуки)

| Сервис | Ответственность | Endpoints |
|--------|-----------------|-----------|
| **Auth Service** | Аутентификация, авторизация, управление аккаунтами | /auth/*, /accounts/* |
| **Trading Service** | Ордера, портфель, проксирование котировок | /orders/*, /portfolio/*, /quotes/* |
| **Broker Service** | Эмуляция Tinkoff API, исполнение ордеров | /broker/*, /instruments/* |

### Взаимодействие

| Связь | Тип | Описание |
|-------|-----|----------|
| Gateway → Services | REST | Маршрутизация запросов |
| Trading → Auth | REST | Валидация токенов |
| Trading → Broker | REST + RabbitMQ | Размещение ордеров, получение котировок |
| Broker → Trading | RabbitMQ | События: OrderExecuted, QuoteUpdated |

### Strategy Service

**Статус:** Опционально (если будет время)

Для защиты достаточно ручной торговли. Автоматические стратегии — для production.

---

## Распределённые транзакции: Saga Pattern

### Проблема

При создании ордера участвуют несколько сервисов:
1. Trading Service — создаёт ордер
2. Broker Service — исполняет ордер
3. Trading Service — обновляет портфель

Если Broker упал после создания ордера — нужен откат.

### Решение: Choreography-based Saga

```
┌─────────────────────────────────────────────────────────────────┐
│                    УСПЕШНЫЙ СЦЕНАРИЙ                            │
└─────────────────────────────────────────────────────────────────┘

  Client          Trading           RabbitMQ          Broker
    │                │                  │                │
    │ POST /orders   │                  │                │
    │───────────────▶│                  │                │
    │                │                  │                │
    │                │ 1. Save order    │                │
    │                │    status=PENDING│                │
    │                │                  │                │
    │                │ 2. Publish       │                │
    │                │──────────────────▶ OrderPlaced   │
    │                │                  │───────────────▶│
    │                │                  │                │
    │ 202 Accepted   │                  │                │ 3. Execute
    │◀───────────────│                  │                │    order
    │                │                  │                │
    │                │                  │  OrderExecuted │
    │                │◀──────────────────────────────────│
    │                │                  │                │
    │                │ 4. Update order  │                │
    │                │    status=FILLED │                │
    │                │                  │                │
    │                │ 5. Update        │                │
    │                │    portfolio     │                │
    │                │                  │                │


┌─────────────────────────────────────────────────────────────────┐
│                    СЦЕНАРИЙ ОТКАТА                              │
└─────────────────────────────────────────────────────────────────┘

  Client          Trading           RabbitMQ          Broker
    │                │                  │                │
    │ POST /orders   │                  │                │
    │───────────────▶│                  │                │
    │                │                  │                │
    │                │ 1. Save order    │                │
    │                │    status=PENDING│                │
    │                │                  │                │
    │                │ 2. Publish       │                │
    │                │──────────────────▶ OrderPlaced   │
    │                │                  │───────────────▶│
    │                │                  │                │
    │ 202 Accepted   │                  │                │ 3. Execution
    │◀───────────────│                  │                │    FAILED!
    │                │                  │                │
    │                │                  │  OrderRejected │
    │                │◀──────────────────────────────────│
    │                │                  │                │
    │                │ 4. COMPENSATE:   │                │
    │                │    status=REJECTED                │
    │                │                  │                │
    │                │ 5. NO portfolio  │                │
    │                │    changes       │                │
    │                │                  │                │
```

### События RabbitMQ

| Exchange | Routing Key | Publisher | Consumer | Описание |
|----------|-------------|-----------|----------|----------|
| orders | order.placed | Trading | Broker | Новый ордер на исполнение |
| orders | order.executed | Broker | Trading | Ордер исполнен |
| orders | order.rejected | Broker | Trading | Ордер отклонён (компенсация) |
| orders | order.cancelled | Trading | Broker | Отмена ордера |
| quotes | quote.updated | Broker | Trading | Обновление котировки |

### Статусы ордера (Saga State Machine)

```
                    ┌─────────┐
                    │   NEW   │
                    └────┬────┘
                         │ publish OrderPlaced
                         ▼
                    ┌─────────┐
         ┌──────────│ PENDING │──────────┐
         │          └─────────┘          │
         │ OrderRejected          OrderExecuted
         ▼                               ▼
    ┌──────────┐                   ┌──────────┐
    │ REJECTED │                   │  FILLED  │
    └──────────┘                   └──────────┘
         
         
                    ┌─────────┐
                    │ PENDING │
                    └────┬────┘
                         │ CancelOrder
                         ▼
                    ┌───────────┐
                    │ CANCELLED │
                    └───────────┘
```

### Тесты на Saga

```cpp
// tests/saga/OrderSagaTest.cpp

// 1. Успешное исполнение ордера
TEST(OrderSaga, SuccessfulExecution) {
    // Given: Trading создаёт ордер
    // When: Broker публикует OrderExecuted
    // Then: Ордер в статусе FILLED, портфель обновлён
}

// 2. Отклонение ордера (недостаточно средств)
TEST(OrderSaga, RejectedInsufficientFunds) {
    // Given: Trading создаёт ордер
    // When: Broker публикует OrderRejected(reason="Insufficient funds")
    // Then: Ордер в статусе REJECTED, портфель НЕ изменён
}

// 3. Отклонение ордера (инструмент не найден)
TEST(OrderSaga, RejectedInstrumentNotFound) {
    // Given: Trading создаёт ордер с неверным FIGI
    // When: Broker публикует OrderRejected(reason="Instrument not found")
    // Then: Ордер в статусе REJECTED
}

// 4. Таймаут исполнения
TEST(OrderSaga, ExecutionTimeout) {
    // Given: Trading создаёт ордер
    // When: Broker не отвечает 30 секунд
    // Then: Ордер в статусе TIMEOUT, можно retry или cancel
}

// 5. Отмена ордера в статусе PENDING
TEST(OrderSaga, CancelPendingOrder) {
    // Given: Ордер в статусе PENDING
    // When: Клиент вызывает DELETE /orders/{id}
    // Then: Публикуется OrderCancelled, статус CANCELLED
}

// 6. Попытка отмены исполненного ордера
TEST(OrderSaga, CannotCancelFilledOrder) {
    // Given: Ордер в статусе FILLED
    // When: Клиент вызывает DELETE /orders/{id}
    // Then: 400 Bad Request, ордер остаётся FILLED
}

// 7. Идемпотентность обработки событий
TEST(OrderSaga, IdempotentEventHandling) {
    // Given: Ордер исполнен
    // When: OrderExecuted приходит повторно (retry)
    // Then: Состояние не меняется, нет дублирования
}
```

---

## UI: Простой SPA

### Страницы

| Страница | Функционал |
|----------|------------|
| `index.html` | Регистрация + Логин |
| `dashboard.html` | Главная: портфель + котировки |
| `orders.html` | Список ордеров + создание |

### Функционал

1. **Регистрация**
   - Форма: username, password
   - POST /api/v1/auth/register

2. **Логин**
   - Форма: username, password
   - POST /api/v1/auth/login → session_token
   - Выбор аккаунта → access_token
   - Сохранение токенов в localStorage

3. **Котировки (real-time эмуляция)**
   - GET /api/v1/quotes?figi=... (polling каждые 5 сек)
   - Таблица: Ticker | Last Price | Bid | Ask | Change

4. **Портфель**
   - GET /api/v1/portfolio
   - Позиции: Ticker | Quantity | Avg Price | Current | P&L
   - Денежные средства: RUB, USD

5. **Создание ордера**
   - Форма: FIGI, Quantity, Direction (BUY/SELL), Type (MARKET/LIMIT)
   - POST /api/v1/orders
   - Показать статус: PENDING → FILLED/REJECTED

6. **Список ордеров**
   - GET /api/v1/orders
   - Таблица: ID | FIGI | Qty | Direction | Status | Created
   - Кнопка "Отменить" для PENDING

7. **Logout**
   - POST /api/v1/auth/logout
   - Очистка localStorage

### Технологии

- Vanilla JS (без фреймворков)
- Fetch API
- CSS Grid/Flexbox
- Никаких npm/webpack

---

## Этапы реализации

### Этап 0: Стабилизация MVP (3-4 часа)

| # | Задача | Время |
|---|--------|-------|
| 0.1 | Регистрация недостающих хэндлеров (Register, Accounts) | 30 мин |
| 0.2 | Проверка docker-compose up | 1 час |
| 0.3 | Исправление GitHub Actions | 1 час |
| 0.4 | Базовый тест EventBus | 30 мин |

### Этап 1: RabbitMQ интеграция (4-5 часов)

| # | Задача | Время |
|---|--------|-------|
| 1.1 | Добавить RabbitMQ в docker-compose | 30 мин |
| 1.2 | Создать IRabbitMQAdapter (port) | 1 час |
| 1.3 | Реализовать RabbitMQAdapter (amqp-cpp) | 2 часа |
| 1.4 | Тесты подключения | 1 час |

### Этап 2: Broker Service (4-5 часов)

| # | Задача | Время |
|---|--------|-------|
| 2.1 | Создать структуру broker-service/ | 30 мин |
| 2.2 | Перенести FakeTinkoffAdapter | 1 час |
| 2.3 | REST endpoints: /quotes, /instruments, /broker/execute | 2 часа |
| 2.4 | Publish events: OrderExecuted, QuoteUpdated | 1 час |

### Этап 3: Auth Service (3-4 часа)

| # | Задача | Время |
|---|--------|-------|
| 3.1 | Создать структуру auth-service/ | 30 мин |
| 3.2 | Перенести Auth handlers + AuthService | 1.5 часа |
| 3.3 | REST endpoint: POST /auth/validate (для Trading) | 1 час |
| 3.4 | Dockerfile + тесты | 1 час |

### Этап 4: Trading Service (4-5 часов)

| # | Задача | Время |
|---|--------|-------|
| 4.1 | Создать структуру trading-service/ | 30 мин |
| 4.2 | Перенести Order, Portfolio handlers | 1.5 часа |
| 4.3 | REST client для Auth.validate() | 1 час |
| 4.4 | REST client для Broker | 1 час |
| 4.5 | Subscribe to RabbitMQ events | 1 час |

### Этап 5: Saga Implementation (3-4 часа)

| # | Задача | Время |
|---|--------|-------|
| 5.1 | OrderSaga state machine в Trading | 1.5 часа |
| 5.2 | Compensating transactions (reject handling) | 1 час |
| 5.3 | Тесты Saga (7 сценариев) | 1.5 часа |

### Этап 6: API Gateway (1-2 часа)

| # | Задача | Время |
|---|--------|-------|
| 6.1 | nginx конфигурация | 30 мин |
| 6.2 | Routing rules | 30 мин |
| 6.3 | Проверка всех endpoints через Gateway | 30 мин |

### Этап 7: UI (4-5 часов)

| # | Задача | Время |
|---|--------|-------|
| 7.1 | index.html: Register + Login | 1 час |
| 7.2 | dashboard.html: Portfolio + Quotes | 1.5 часа |
| 7.3 | orders.html: Order list + Create | 1.5 часа |
| 7.4 | Стили CSS | 1 час |

### Этап 8: PostgreSQL (3-4 часа) — В КОНЦЕ

| # | Задача | Время |
|---|--------|-------|
| 8.1 | SQL схемы для каждой БД | 1 час |
| 8.2 | PostgresUserRepository | 1 час |
| 8.3 | PostgresOrderRepository | 1 час |
| 8.4 | Миграции (init.sql) | 30 мин |

### Этап 9: Финализация (2-3 часа)

| # | Задача | Время |
|---|--------|-------|
| 9.1 | Полный docker-compose с 3 сервисами | 1 час |
| 9.2 | README с инструкциями запуска | 30 мин |
| 9.3 | Архитектурная документация | 1 час |
| 9.4 | Демо-сценарий для защиты | 30 мин |

---

## Суммарная оценка

| Этап | Время |
|------|-------|
| 0. Стабилизация MVP | 3-4 часа |
| 1. RabbitMQ | 4-5 часов |
| 2. Broker Service | 4-5 часов |
| 3. Auth Service | 3-4 часа |
| 4. Trading Service | 4-5 часов |
| 5. Saga | 3-4 часа |
| 6. API Gateway | 1-2 часа |
| 7. UI | 4-5 часов |
| 8. PostgreSQL | 3-4 часа |
| 9. Финализация | 2-3 часа |
| **ИТОГО** | **~32-40 часов** |

При интенсивной работе: **3-4 дня**.

---

## Обновлённый план на 3 дня (приоритеты)

### День 1: Стабилизация + Инфраструктура (~10-12 часов)
1.1 Доработка TradingApp.cpp (хэндлеры) 30 мин (done)
1.2 Тесты на новые хэндлеры (Account) 1 час (done)
1.3 Исправление CI (GitHub Actions) 1 час (done - тесты собираются, отчет формируется с docker есть проблемы, оставим на тех.долг)
1.4 Проверка docker-compose up 1 час (done)
1.5 MetricsHandler — привести в порядок 1 час (done)
1.6 RabbitMQ: добавить в docker-compose 30 мин (done)
1.7 RabbitMQ: IRabbitMQAdapter + реализация 2-3 часа (done)
1.8 Prometheus: проверить scraping 30 мин (done)
1.9 PostgreSQL: схемы + репозитории 3-4 часа (done)

### День 2: Распил на микросервисы (~10-12 часов)
2.1 Auth Service (структура + код) 2-3 часа
2.2 Broker Service (структура + код) 2-3 часа
2.3 Trading Service (структура + код) 3-4 часа
2.4 API Gateway (nginx) 1 час
2.5 Docker Compose для 3 сервисов 1-2 часа

### День 3: Saga + UI + Финализация (~10-12 часов)
3.1 Saga: OrderPlaced → OrderExecuted/Rejected 2-3 часа
3.2 Тесты на Saga (5-7 сценариев) 2 часа
3.3 UI: Register + Login 1.5 часа
3.4 UI: Portfolio + Quotes 1.5 часа
3.5 UI: Orders (list + create + cancel) 1.5 часа
3.6 Документация + демо-сценарий 1-2 часа

---

## Зависимости и библиотеки

### RabbitMQ Client (C++)

Рекомендую **amqp-cpp** (header-only, работает с Boost.Asio):

```cmake
FetchContent_Declare(
    amqpcpp
    GIT_REPOSITORY https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git
    GIT_TAG v4.3.26
)
FetchContent_MakeAvailable(amqpcpp)
```

### HTTP Client (для REST между сервисами)

Используем уже имеющийся Boost.Beast или добавляем простой wrapper.

---

## Риски и митигация

| Риск | Вероятность | Митигация |
|------|-------------|-----------|
| RabbitMQ сложнее, чем ожидалось | Средняя | Fallback на REST-only (показать как TODO) |
| Не хватает времени на PostgreSQL | Высокая | InMemory достаточно для демо |
| Saga слишком сложная | Низкая | Упростить до 3 сценариев |
| UI занимает много времени | Средняя | Минимальный функционал |

---

## Демо-сценарий для защиты

1. **Показать архитектуру** (диаграмма)
2. **Запустить docker-compose up**
3. **Открыть UI:**
   - Зарегистрироваться
   - Войти
   - Показать котировки
   - Создать ордер на покупку
   - Показать статус PENDING → FILLED
   - Показать обновлённый портфель
4. **Показать RabbitMQ:**
   - Management UI (порт 15672)
   - Очереди, сообщения
5. **Показать Saga:**
   - Создать ордер с недостаточными средствами
   - Показать REJECTED статус
   - Объяснить компенсирующую транзакцию
6. **Показать Prometheus:**
   - Метрики /metrics
   - Графики (если есть Grafana)

---
