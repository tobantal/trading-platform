# Trading Service

Публичный API для торговой платформы. Авторизует запросы через auth-service, кэширует рыночные данные, управляет ордерами через RabbitMQ.

## Архитектура

```
User → trading-service (Bearer token)
           ├─► auth-service (валидация токена → account_id)
           ├─► broker-service (HTTP: котировки, инструменты, портфель)
           └─► RabbitMQ (order.create/cancel → broker-service)
                   └─► broker-service слушает, исполняет, публикует order.created/rejected
```

## API Endpoints

### Без авторизации

| Method | Endpoint | Описание |
|--------|----------|----------|
| GET | `/health` | Health check |
| GET | `/api/v1/instruments` | Список инструментов |
| GET | `/api/v1/instruments/{figi}` | Инструмент по FIGI |
| GET | `/api/v1/instruments/search?query=` | Поиск инструментов |
| GET | `/api/v1/quotes?figis=` | Котировки |

### С авторизацией (Bearer access_token)

| Method | Endpoint | Описание |
|--------|----------|----------|
| POST | `/api/v1/orders` | Создать ордер |
| GET | `/api/v1/orders` | Список ордеров |
| GET | `/api/v1/orders/{id}` | Ордер по ID |
| DELETE | `/api/v1/orders/{id}` | Отменить ордер |
| GET | `/api/v1/portfolio` | Портфель |
| GET | `/api/v1/portfolio/positions` | Позиции |
| GET | `/api/v1/portfolio/cash` | Баланс |

## Сборка

```bash
# Локальная сборка
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# С тестами
cmake -DBUILD_TESTS=ON ..
make -j$(nproc)
ctest --output-on-failure

# Docker
docker build -t tobantal/trading-service:latest -f trading-service/Dockerfile .
docker push tobantal/trading-service:latest
```

## Деплой в Kubernetes

```bash
# Применить deployment (из корня проекта)
kubectl apply -f k8s/trading-service.yaml

# Проверка
kubectl get pods -n trading -l app=trading-service
kubectl logs -f deployment/trading-service -n trading

# Port-forward для тестирования
kubectl port-forward svc/trading-service 8082:8082 -n trading
```

## Переменные окружения

| Переменная | Default | Описание |
|------------|---------|----------|
| `HTTP_PORT` | 8082 | Порт сервера |
| `AUTH_SERVICE_HOST` | auth-service | Хост auth-service |
| `AUTH_SERVICE_PORT` | 8081 | Порт auth-service |
| `BROKER_SERVICE_HOST` | broker-service | Хост broker-service |
| `BROKER_SERVICE_PORT` | 8083 | Порт broker-service |
| `RABBITMQ_HOST` | rabbitmq | Хост RabbitMQ |
| `RABBITMQ_PORT` | 5672 | Порт RabbitMQ |
| `RABBITMQ_USER` | guest | Пользователь RabbitMQ |
| `RABBITMQ_PASSWORD` | guest | Пароль RabbitMQ |
| `RABBITMQ_EXCHANGE` | trading.events | Exchange |
| `CACHE_QUOTE_SIZE` | 1000 | Размер кэша котировок |
| `CACHE_QUOTE_TTL_SECONDS` | 10 | TTL котировок |
| `CACHE_INSTRUMENT_SIZE` | 500 | Размер кэша инструментов |
| `CACHE_INSTRUMENT_TTL_SECONDS` | 3600 | TTL инструментов |

## RabbitMQ Events

**Публикует:** `order.create`, `order.cancel`  
**Слушает:** `order.created`, `order.rejected`, `order.filled`, `order.cancelled`

## Зависимости

- auth-service (порт 8081)
- broker-service (порт 8083)
- RabbitMQ (порт 5672)
