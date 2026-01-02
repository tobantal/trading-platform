# Broker Service

Микросервис симуляции брокера для торговой платформы.

## Сборка и запуск

### 1. Сборка проекта

```bash
# Из корня проекта
mkdir -p build && cd build
cmake .. -DBUILD_TESTS=ON
make broker-service -j4
make broker-service-tests -j4

# Запуск unit-тестов
./education/broker-service/broker-service-tests
```

### 2. Сборка Docker образа

```bash
# Из корня проекта (где лежит CMakeLists.txt)
docker build -t tobantal/broker-service:latest -f broker-service/Dockerfile .
docker push tobantal/broker-service:latest
```

### 3. Деплой в Kubernetes

```bash
# Namespace и секреты (если еще не созданы)
kubectl apply -f k8s/namespace.yaml
kubectl apply -f k8s/secret.yaml

# Инфраструктура
kubectl apply -f k8s/broker-postgres.yaml
kubectl apply -f k8s/rabbitmq.yaml

# Сервис
kubectl apply -f k8s/broker-service.yaml

# Ingress (если еще не создан)
kubectl apply -f k8s/ingress.yaml

# Проверка статуса
kubectl get pods -n trading -l app=broker-service
kubectl logs -n trading -l app=broker-service -f
```

### 4. Проверка работоспособности

```bash
# Health check
curl http://arch.homework/broker/health

# Metrics
curl http://arch.homework/broker/metrics

# Список инструментов
curl http://arch.homework/broker/api/v1/instruments

# Котировка
curl http://arch.homework/broker/api/v1/quotes?figi=BBG004730N88
```

### 5. Запуск Postman тестов

```bash
# Установка Newman (если еще не установлен)
npm install -g newman
npm install -g newman-reporter-htmlextra

# Запуск тестов
newman run postman/broker-service.postman_collection.json

# С HTML отчетом
newman run postman/broker-service.postman_collection.json \
  -r htmlextra \
  --reporter-htmlextra-export reports/broker-service-report.html
```

## API Endpoints

| Метод | Путь | Описание |
|-------|------|----------|
| GET | /health | Health check |
| GET | /metrics | Prometheus metrics |
| GET | /api/v1/instruments | Список инструментов |
| GET | /api/v1/instruments/{figi} | Инструмент по FIGI |
| GET | /api/v1/quotes?figi={figi} | Котировка по FIGI |

### Примеры запросов

```bash
# Health
curl http://arch.homework/broker/health

# Все инструменты
curl http://arch.homework/broker/api/v1/instruments

# Инструмент по FIGI
curl http://arch.homework/broker/api/v1/instruments/BBG004730N88

# Котировка SBER
curl http://arch.homework/broker/api/v1/quotes?figi=BBG004730N88
```

## Архитектура

```
Trading Service                           Broker Service
    |                                        |
    | RabbitMQ: order.placed -----------> OrderEventHandler
    |<---------- order.executed ----------|   |
    |<---------- order.rejected ----------|   v
    |<---------- order.partially_filled---|  OrderExecutor
                                         |   |
                                         |   v
                                         | FakeBroker (симуляция)
                                         |   +-- PriceSimulator
                                         |   +-- OrderProcessor
                                         |   +-- BackgroundTicker
                                         |
                                         v
                                   broker_db
```

## Компоненты симуляции

| Компонент | Описание |
|-----------|----------|
| PriceSimulator | Генерация цен (random walk) |
| OrderProcessor | Обработка ордеров со сценариями |
| BackgroundTicker | Фоновое обновление цен |
| MarketScenario | Настройка поведения рынка |
| EnhancedFakeBroker | Полнофункциональный fake-брокер |

## Конфигурация

### Переменные окружения

| Переменная | По умолчанию | Описание |
|------------|--------------|----------|
| HTTP_PORT | 8083 | Порт HTTP сервера |
| DB_HOST | localhost | Хост PostgreSQL |
| DB_PORT | 5432 | Порт PostgreSQL |
| DB_NAME | broker_db | Имя БД |
| DB_USER | broker | Пользователь БД |
| DB_PASSWORD | broker123 | Пароль БД |
| RABBITMQ_HOST | localhost | Хост RabbitMQ |
| RABBITMQ_PORT | 5672 | Порт RabbitMQ |

### Настройки симуляции

| Переменная | По умолчанию | Описание |
|------------|--------------|----------|
| BROKER_FILL_BEHAVIOR | REALISTIC | Режим исполнения |
| BROKER_SLIPPAGE | 0.001 | Проскальзывание (0.1%) |
| BROKER_PARTIAL_RATIO | 0.5 | Коэффициент частичного исполнения |
| BROKER_TICK_INTERVAL_MS | 100 | Интервал тиков (мс) |

### Режимы исполнения (BROKER_FILL_BEHAVIOR)

| Режим | Описание |
|-------|----------|
| IMMEDIATE | Мгновенное исполнение по текущей цене |
| REALISTIC | Market сразу, Limit ждут цену |
| PARTIAL | Частичное исполнение |
| ALWAYS_REJECT | Всегда отклонять (для тестов ошибок) |

## Тестовые данные

### Инструменты

| FIGI | Тикер | Название | Лот | Цена |
|------|-------|----------|-----|------|
| BBG004730N88 | SBER | Сбербанк | 10 | ~265 RUB |
| BBG004730RP0 | GAZP | Газпром | 10 | ~128 RUB |
| BBG004731032 | LKOH | Лукойл | 1 | ~7200 RUB |
| BBG004730ZJ9 | VTBR | ВТБ | 10000 | ~0.02 RUB |
| BBG006L8G4H1 | YNDX | Яндекс | 1 | ~3500 RUB |

### Тестовые аккаунты

| Account ID | Пользователь | Баланс |
|------------|--------------|--------|
| acc-001-sandbox | trader1 | 1 000 000 RUB |
| acc-001-prod | trader1 | 500 000 RUB |
| acc-002-sandbox | trader2 | 100 000 RUB |
| acc-004-sandbox | admin | 10 000 000 RUB |

## RabbitMQ события

### Входящие (от Trading Service)

| Событие | Описание |
|---------|----------|
| order.placed | Новый ордер размещен |
| order.cancelled | Ордер отменен |

### Исходящие (к Trading Service)

| Событие | Описание |
|---------|----------|
| order.executed | Ордер исполнен полностью |
| order.partially_filled | Ордер исполнен частично |
| order.rejected | Ордер отклонен |

## Структура проекта

```
broker-service/
+-- include/
|   +-- adapters/
|   |   +-- primary/           # HTTP handlers
|   |   +-- secondary/         # DB, RabbitMQ, FakeBroker
|   |       +-- broker/        # Симуляция брокера
|   +-- application/           # Бизнес-логика
|   +-- domain/                # Сущности
|   +-- ports/                 # Интерфейсы
|   +-- settings/              # Настройки из ENV
+-- src/
+-- tests/
+-- sql/
+-- postman/
+-- Dockerfile
+-- CMakeLists.txt
+-- README.md
```
