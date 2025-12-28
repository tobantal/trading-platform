# API Endpoints

> Последнее обновление: 2025-12-28  
> Версия API: v1  
> Base URL: `http://localhost:8080/api/v1`

## Содержание

1. [Обзор](#обзор)
2. [Авторизация](#авторизация)
3. [Auth Endpoints](#auth-endpoints)
4. [Account Endpoints](#account-endpoints)
5. [Market Endpoints](#market-endpoints)
6. [Order Endpoints](#order-endpoints)
7. [Portfolio Endpoints](#portfolio-endpoints)
8. [Strategy Endpoints](#strategy-endpoints)
9. [System Endpoints](#system-endpoints)
10. [Полный тестовый сценарий](#полный-тестовый-сценарий)

---

## Обзор

### Типы токенов

| Тип | Получение | Время жизни | Использование |
|-----|-----------|-------------|---------------|
| Session Token | POST /auth/login | 24 часа | Управление аккаунтами, refresh |
| Access Token | POST /auth/select-account | 1 час | Все торговые операции |

### Статусы ответов

| Код | Описание |
|-----|----------|
| 200 | OK |
| 201 | Created |
| 400 | Bad Request |
| 401 | Unauthorized |
| 403 | Forbidden |
| 404 | Not Found |
| 500 | Internal Server Error |

---

## Авторизация

Все защищённые endpoints требуют заголовок:
```
Authorization: Bearer <token>
```

| Endpoint | Требуемый токен |
|----------|-----------------|
| /auth/register | ❌ Не требуется |
| /auth/login | ❌ Не требуется |
| /auth/select-account | 🔑 Session Token |
| /auth/refresh | 🔑 Session Token |
| /auth/logout | 🔑 Любой токен |
| /accounts/* | 🔑 Session Token |
| Все остальные | 🔐 Access Token |

---

## Auth Endpoints

### POST /api/v1/auth/register

Регистрация нового пользователя.

**Статус:** ⚠️ Не зарегистрирован в TradingApp (TD-007)

```bash
curl -X POST http://localhost:8080/api/v1/auth/register \
  -H "Content-Type: application/json" \
  -d '{
    "username": "trader1",
    "password": "secret123"
  }'
```

**Response 201:**
```json
{
  "user_id": "user-12345678",
  "message": "User registered successfully"
}
```

---

### POST /api/v1/auth/login

Вход в систему. Возвращает session_token.

```bash
curl -X POST http://localhost:8080/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{
    "username": "trader1",
    "password": "secret123"
  }'
```

**Response 200:**
```json
{
  "session_token": "eyJhbGciOiJIUzI1NiIs...",
  "token_type": "Bearer",
  "expires_in": 86400,
  "user": {
    "id": "user-12345678",
    "username": "trader1"
  },
  "accounts": [
    {
      "id": "acc-87654321",
      "name": "Мой счёт",
      "type": "SANDBOX",
      "active": true
    }
  ]
}
```

---

### POST /api/v1/auth/select-account

Выбор аккаунта для работы. Возвращает access_token.

```bash
# Сохраняем session token
SESSION_TOKEN="eyJhbGciOiJIUzI1NiIs..."

curl -X POST http://localhost:8080/api/v1/auth/select-account \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $SESSION_TOKEN" \
  -d '{
    "account_id": "acc-87654321"
  }'
```

**Response 200:**
```json
{
  "access_token": "eyJhbGciOiJIUzI1NiIs...",
  "token_type": "Bearer",
  "expires_in": 3600,
  "account": {
    "id": "acc-87654321",
    "name": "Мой счёт",
    "type": "SANDBOX",
    "active": true
  }
}
```

---

### POST /api/v1/auth/validate

Валидация токена.

```bash
curl -X POST http://localhost:8080/api/v1/auth/validate \
  -H "Content-Type: application/json" \
  -d '{
    "token": "eyJhbGciOiJIUzI1NiIs..."
  }'
```

**Response 200:**
```json
{
  "valid": true,
  "token_type": "access",
  "user_id": "user-12345678",
  "username": "trader1",
  "account_id": "acc-87654321",
  "remaining_seconds": 3540
}
```

---

### POST /api/v1/auth/refresh

Обновление session token.

```bash
curl -X POST http://localhost:8080/api/v1/auth/refresh \
  -H "Authorization: Bearer $SESSION_TOKEN"
```

**Response 200:**
```json
{
  "session_token": "eyJhbGciOiJIUzI1NiIs...",
  "token_type": "Bearer",
  "expires_in": 86400
}
```

---

### POST /api/v1/auth/logout

Выход из системы.

```bash
# Выход из текущей сессии
curl -X POST http://localhost:8080/api/v1/auth/logout \
  -H "Authorization: Bearer $ACCESS_TOKEN"

# Выход из всех сессий
curl -X POST http://localhost:8080/api/v1/auth/logout \
  -H "Authorization: Bearer $ACCESS_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"logout_all": true}'
```

---

## Account Endpoints

**Статус:** ⚠️ Не зарегистрированы в TradingApp (TD-007)

### GET /api/v1/accounts

Список аккаунтов пользователя.

```bash
curl -X GET http://localhost:8080/api/v1/accounts \
  -H "Authorization: Bearer $SESSION_TOKEN"
```

---

### POST /api/v1/accounts

Привязать аккаунт.

```bash
curl -X POST http://localhost:8080/api/v1/accounts \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $SESSION_TOKEN" \
  -d '{
    "name": "Мой sandbox",
    "type": "SANDBOX",
    "broker_token": "t.sandbox-token"
  }'
```

---

### DELETE /api/v1/accounts/{id}

Отвязать аккаунт.

```bash
curl -X DELETE http://localhost:8080/api/v1/accounts/acc-12345678 \
  -H "Authorization: Bearer $SESSION_TOKEN"
```

---

## Market Endpoints

### GET /api/v1/quotes?figi={figi}

Получить котировку.

```bash
ACCESS_TOKEN="eyJhbGciOiJIUzI1NiIs..."

curl -X GET "http://localhost:8080/api/v1/quotes?figi=BBG004730N88" \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

**Response 200:**
```json
{
  "figi": "BBG004730N88",
  "ticker": "SBER",
  "last_price": {"units": 250, "nano": 500000000, "currency": "RUB"},
  "bid_price": {"units": 250, "nano": 400000000, "currency": "RUB"},
  "ask_price": {"units": 250, "nano": 600000000, "currency": "RUB"},
  "updated_at": "2025-12-28T12:00:00Z"
}
```

---

### GET /api/v1/instruments

Список всех инструментов.

```bash
curl -X GET http://localhost:8080/api/v1/instruments \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

---

### GET /api/v1/instruments/search?query={query}

Поиск инструментов.

```bash
curl -X GET "http://localhost:8080/api/v1/instruments/search?query=SBER" \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

---

### GET /api/v1/instruments/{figi}

Получить инструмент по FIGI.

```bash
curl -X GET http://localhost:8080/api/v1/instruments/BBG004730N88 \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

---

## Order Endpoints

### POST /api/v1/orders

Создать ордер.

```bash
curl -X POST http://localhost:8080/api/v1/orders \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $ACCESS_TOKEN" \
  -d '{
    "figi": "BBG004730N88",
    "quantity": 10,
    "direction": "BUY",
    "order_type": "MARKET"
  }'
```

**Response 201:**
```json
{
  "order_id": "ord-12345678",
  "status": "NEW",
  "message": "Order created successfully"
}
```

---

### GET /api/v1/orders

Список ордеров.

```bash
curl -X GET http://localhost:8080/api/v1/orders \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

---

### GET /api/v1/orders/{id}

Получить ордер по ID.

```bash
curl -X GET http://localhost:8080/api/v1/orders/ord-12345678 \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

---

### DELETE /api/v1/orders/{id}

Отменить ордер.

```bash
curl -X DELETE http://localhost:8080/api/v1/orders/ord-12345678 \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

---

## Portfolio Endpoints

### GET /api/v1/portfolio

Получить портфель.

```bash
curl -X GET http://localhost:8080/api/v1/portfolio \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

---

### GET /api/v1/portfolio/positions

Получить позиции.

```bash
curl -X GET http://localhost:8080/api/v1/portfolio/positions \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

---

### GET /api/v1/portfolio/cash

Получить денежные средства.

```bash
curl -X GET http://localhost:8080/api/v1/portfolio/cash \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

---

## Strategy Endpoints

### POST /api/v1/strategies

Создать стратегию.

```bash
curl -X POST http://localhost:8080/api/v1/strategies \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $ACCESS_TOKEN" \
  -d '{
    "name": "SMA Strategy",
    "type": "SMA_CROSSOVER",
    "figi": "BBG004730N88",
    "config": {
      "short_period": 10,
      "long_period": 30
    }
  }'
```

---

### GET /api/v1/strategies

Список стратегий.

```bash
curl -X GET http://localhost:8080/api/v1/strategies \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

---

### GET /api/v1/strategies/{id}

Получить стратегию по ID.

```bash
curl -X GET http://localhost:8080/api/v1/strategies/str-12345678 \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

---

### POST /api/v1/strategies/{id}/start

Запустить стратегию.

```bash
curl -X POST http://localhost:8080/api/v1/strategies/str-12345678/start \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

---

### POST /api/v1/strategies/{id}/stop

Остановить стратегию.

```bash
curl -X POST http://localhost:8080/api/v1/strategies/str-12345678/stop \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

---

### DELETE /api/v1/strategies/{id}

Удалить стратегию.

```bash
curl -X DELETE http://localhost:8080/api/v1/strategies/str-12345678 \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

---

## System Endpoints

### GET /api/v1/health

Проверка здоровья сервера.

```bash
curl -X GET http://localhost:8080/api/v1/health
```

**Response 200:**
```json
{
  "status": "healthy",
  "timestamp": "2025-12-28T12:00:00Z"
}
```

---

### GET /metrics

Метрики Prometheus.

```bash
curl -X GET http://localhost:8080/metrics
```

---

## Полный тестовый сценарий

```bash
#!/bin/bash
# Полный тестовый флоу

BASE_URL="http://localhost:8080/api/v1"

echo "=== 1. Health Check ==="
curl -s "$BASE_URL/health" | jq .

echo -e "\n=== 2. Login ==="
LOGIN_RESPONSE=$(curl -s -X POST "$BASE_URL/auth/login" \
  -H "Content-Type: application/json" \
  -d '{"username": "testuser", "password": "password123"}')
echo $LOGIN_RESPONSE | jq .

SESSION_TOKEN=$(echo $LOGIN_RESPONSE | jq -r '.session_token')
ACCOUNT_ID=$(echo $LOGIN_RESPONSE | jq -r '.accounts[0].id')

echo -e "\n=== 3. Select Account ==="
SELECT_RESPONSE=$(curl -s -X POST "$BASE_URL/auth/select-account" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $SESSION_TOKEN" \
  -d "{\"account_id\": \"$ACCOUNT_ID\"}")
echo $SELECT_RESPONSE | jq .

ACCESS_TOKEN=$(echo $SELECT_RESPONSE | jq -r '.access_token')

echo -e "\n=== 4. Get Quote ==="
curl -s "$BASE_URL/quotes?figi=BBG004730N88" \
  -H "Authorization: Bearer $ACCESS_TOKEN" | jq .

echo -e "\n=== 5. Get Portfolio ==="
curl -s "$BASE_URL/portfolio" \
  -H "Authorization: Bearer $ACCESS_TOKEN" | jq .

echo -e "\n=== 6. Create Order ==="
curl -s -X POST "$BASE_URL/orders" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $ACCESS_TOKEN" \
  -d '{
    "figi": "BBG004730N88",
    "quantity": 10,
    "direction": "BUY",
    "order_type": "MARKET"
  }' | jq .

echo -e "\n=== 7. Logout ==="
curl -s -X POST "$BASE_URL/auth/logout" \
  -H "Authorization: Bearer $ACCESS_TOKEN" | jq .

echo -e "\n=== Done! ==="
```

---

## Сводная таблица Endpoints

| # | Метод | Путь | Auth | Статус |
|---|-------|------|------|--------|
| 1 | POST | /auth/register | ❌ | ⚠️ Не зарегистрирован |
| 2 | POST | /auth/login | ❌ | ✅ |
| 3 | POST | /auth/select-account | Session | ✅ |
| 4 | POST | /auth/validate | ❌ | ✅ |
| 5 | POST | /auth/refresh | Session | ✅ |
| 6 | POST | /auth/logout | Any | ✅ |
| 7 | GET | /accounts | Session | ⚠️ Не зарегистрирован |
| 8 | POST | /accounts | Session | ⚠️ Не зарегистрирован |
| 9 | DELETE | /accounts/{id} | Session | ⚠️ Не зарегистрирован |
| 10 | GET | /quotes | Access | ✅ |
| 11 | GET | /instruments | Access | ✅ |
| 12 | GET | /instruments/search | Access | ✅ |
| 13 | GET | /instruments/{figi} | Access | ✅ |
| 14 | POST | /orders | Access | ✅ |
| 15 | GET | /orders | Access | ✅ |
| 16 | GET | /orders/{id} | Access | ✅ |
| 17 | DELETE | /orders/{id} | Access | ✅ |
| 18 | GET | /portfolio | Access | ✅ |
| 19 | GET | /portfolio/positions | Access | ✅ |
| 20 | GET | /portfolio/cash | Access | ✅ |
| 21 | POST | /strategies | Access | ✅ |
| 22 | GET | /strategies | Access | ✅ |
| 23 | GET | /strategies/{id} | Access | ✅ |
| 24 | POST | /strategies/{id}/start | Access | ✅ |
| 25 | POST | /strategies/{id}/stop | Access | ✅ |
| 26 | DELETE | /strategies/{id} | Access | ✅ |
| 27 | GET | /health | ❌ | ✅ |
| 28 | GET | /metrics | ❌ | ✅ |
