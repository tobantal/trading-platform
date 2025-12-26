# Тестирование API Trading Platform MVP

## 🚀 Быстрый старт

# 1. Запустите сервисы
```bash
docker-compose up -d
```

# 2. Проверьте, что всё работает
curl http://localhost:3001/api/v1/health

🔐 Авторизация
Логин (создание пользователя)
```bash
curl -X POST http://localhost:3001/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "trader1"}'
```
Ответ:
```json
 {
   "access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
   "token_type": "Bearer",
   "expires_in": 3600
 }
```

Валидация токена
```bash
curl -X POST http://localhost:3001/api/v1/auth/validate \
  -H "Content-Type: application/json" \
  -d '{"token": "ВАШ_JWT_ТОКЕН"}'
```

Ответ:
```json
   "valid": true,
   "user_id": "user-001",
   "username": "trader1"
 }
```

📊 Рыночные данные
Получить все котировки
```bash
curl "http://localhost:3001/api/v1/quotes"
```

# ИЛИ получить конкретные инструменты (правильный параметр `figis`):
```bash
curl "http://localhost:3001/api/v1/quotes?figis=BBG004730N88,BBG004731032"
```

Поиск инструментов
```bash
curl "http://localhost:3001/api/v1/instruments/search?query=SBER"
curl "http://localhost:3001/api/v1/instruments/search?query=ЛУКОЙЛ"
```

Получить все инструменты
```bash
curl "http://localhost:3001/api/v1/instruments"
```

Получить информацию по конкретному FIGI
```bash
curl "http://localhost:3001/api/v1/instruments/BBG004730N88"
```

💼 Ордера (требуется авторизация)
Создать ордер
```bash
# MARKET ордер (исполняется по текущей цене)
curl -X POST http://localhost:3001/api/v1/orders \
  -H "Authorization: Bearer ВАШ_JWT_ТОКЕН" \
  -H "Content-Type: application/json" \
  -d '{
    "figi": "BBG004730N88",
    "direction": "BUY",
    "type": "MARKET",
    "quantity": 10
  }'

# LIMIT ордер (указать цену)
curl -X POST http://localhost:3001/api/v1/orders \
  -H "Authorization: Bearer ВАШ_JWT_ТОКЕН" \
  -H "Content-Type: application/json" \
  -d '{
    "figi": "BBG004730N88",
    "direction": "BUY",
    "type": "LIMIT",
    "quantity": 10,
    "price": 260.0,
    "currency": "RUB"
  }'
```

Получить все ордера
```bash
curl "http://localhost:3001/api/v1/orders" \
  -H "Authorization: Bearer ВАШ_JWT_ТОКЕН"
```

Получить конкретный ордер
```bash
curl "http://localhost:3001/api/v1/orders/НАЗНАЧЕННЫЙ_ID" \
  -H "Authorization: Bearer ВАШ_JWT_ТОКЕН"
```

Отменить ордер
```bash
curl -X DELETE "http://localhost:3001/api/v1/orders/НАЗНАЧЕННЫЙ_ID" \
  -H "Authorization: Bearer ВАШ_JWT_ТОКЕН"
```

📈 Портфель
Получить весь портфель
```bash
curl "http://localhost:3001/api/v1/portfolio" \
  -H "Authorization: Bearer ВАШ_JWT_ТОКЕН"
  ```

Получить только позиции
```bash
curl "http://localhost:3001/api/v1/portfolio/positions" \
  -H "Authorization: Bearer ВАШ_JWT_ТОКЕН"
  ```

Получить доступные средства
```bash

curl "http://localhost:3001/api/v1/portfolio/cash" \
  -H "Authorization: Bearer ВАШ_JWT_ТОКЕН"
```

🤖 Торговые стратегии
Создать стратегию
```bash
curl -X POST http://localhost:3001/api/v1/strategies \
  -H "Authorization: Bearer ВАШ_JWT_ТОКЕН" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "SMA Crossover SBER",
    "figi": "BBG004730N88",
    "type": "SMA_CROSSOVER",
    "short_period": 10,
    "long_period": 30,
    "quantity": 1
  }'
  ```

Получить все стратегии
```bash
curl "http://localhost:3001/api/v1/strategies" \
  -H "Authorization: Bearer ВАШ_JWT_ТОКЕН"
```

Получить конкретную стратегию
```bash
curl "http://localhost:3001/api/v1/strategies/НАЗНАЧЕННЫЙ_ID" \
  -H "Authorization: Bearer ВАШ_JWT_ТОКЕН"
```

Запустить стратегию
```bash
curl -X POST "http://localhost:3001/api/v1/strategies/НАЗНАЧЕННЫЙ_ID/start" \
  -H "Authorization: Bearer ВАШ_JWT_ТОКЕН"
```

Остановить стратегию
```bash
curl -X POST "http://localhost:3001/api/v1/strategies/НАЗНАЧЕННЫЙ_ID/stop" \
  -H "Authorization: Bearer ВАШ_JWT_ТОКЕН"
```

Удалить стратегию
```bash
curl -X DELETE "http://localhost:3001/api/v1/strategies/НАЗНАЧЕННЫЙ_ID" \
  -H "Authorization: Bearer ВАШ_JWT_ТОКЕН"
```

📊 Мониторинг
Health check
```bash
curl http://localhost:3001/api/v1/health
```

Prometheus метрики
```bash
curl http://localhost:3001/metrics
```

🐛 Отладка
Проверить конфигурацию
```bash
curl http://localhost:3001/api/v1/health | jq '.services'
```

Просмотр логов контейнеров
```bash
docker-compose logs -f backend
docker-compose logs -f postgres
```

🧪 Тестовые пользователи

В системе предустановлены пользователи:

    trader1 - обычный трейдер

    trader2 - обычный трейдер

    admin - администратор

Для каждого автоматически создаётся sandbox счёт с 1 000 000 ₽.
