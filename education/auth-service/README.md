# Auth Service

Микросервис аутентификации и управления пользовательскими аккаунтами.

## Функциональность

- Регистрация пользователей
- Аутентификация (login/logout)
- JWT токены (session и access)
- Управление брокерскими аккаунтами (sandbox/real)
- Валидация токенов (внутренний API для других сервисов)

## API Endpoints

| Method | Endpoint | Описание | Auth |
|--------|----------|----------|------|
| GET | `/health` | Health check | - |
| GET | `/metrics` | Prometheus metrics | - |
| POST | `/api/v1/auth/register` | Регистрация | - |
| POST | `/api/v1/auth/login` | Логин | - |
| POST | `/api/v1/auth/logout` | Выход | Session Token |
| POST | `/api/v1/auth/validate` | Валидация токена | - |
| GET | `/api/v1/accounts` | Список аккаунтов | Session Token |
| POST | `/api/v1/accounts` | Создать аккаунт | Session Token |
| DELETE | `/api/v1/accounts/{id}` | Удалить аккаунт | Session Token |

## Структура проекта

```
auth-service/
├── CMakeLists.txt
├── Dockerfile
├── config.json
├── README.md
├── include/
│   ├── AuthApp.hpp
│   ├── adapters/
│   │   ├── primary/           # HTTP handlers
│   │   └── secondary/         # PostgreSQL, JWT
│   ├── application/           # Business logic
│   ├── domain/                # Entities, enums
│   └── ports/                 # Interfaces
├── src/
│   ├── main.cpp
│   └── AuthApp.cpp
├── sql/
│   └── init.sql               # В k8s/auth-postgres.yaml
└── tests/
    ├── mocks/                 # InMemory repositories
    ├── AuthServiceTest.cpp
    ├── AccountServiceTest.cpp
    └── AuthEndpointTest.cpp
```

## Локальная разработка

### Требования

- CMake 3.16+
- GCC 12+ или Clang 14+
- PostgreSQL 15
- Docker (опционально)

### Сборка

```bash
# Из корня проекта (course-project/)
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
make -j$(nproc) auth-service auth-service-tests
```

### Запуск тестов

```bash
cd build
ctest --output-on-failure
# или
./auth-service-tests
```

### Запуск локально

```bash
# Установить переменные окружения
export AUTH_DB_HOST=localhost
export AUTH_DB_PORT=5432
export AUTH_DB_NAME=auth_db
export AUTH_DB_USER=auth_user
export AUTH_DB_PASSWORD=secret

# Запуск
./auth-service
```

## Kubernetes

### 1. Сборка и публикация Docker образа

```bash
# Из корня проекта (course-project/)
docker build -t tobantal/auth-service:latest -f auth-service/Dockerfile .

# Логин в DockerHub
docker login

# Публикация образа
docker push tobantal/auth-service:latest
```

### 2. Развёртывание

```bash
# Из корня проекта
kubectl apply -f k8s/namespace.yaml
kubectl apply -f k8s/secret.yaml
kubectl apply -f k8s/auth-postgres.yaml
kubectl apply -f k8s/auth-service.yaml
kubectl apply -f k8s/ingress.yaml
```

### Проверка

```bash
# Статус подов
kubectl get pods -n trading

# Логи
kubectl logs -n trading -l app=auth-service

# Health check
curl http://arch.homework/auth/health
```

## База данных

### Схема

```sql
-- users
CREATE TABLE users (
    user_id VARCHAR(64) PRIMARY KEY,
    username VARCHAR(64) UNIQUE NOT NULL,
    email VARCHAR(128) UNIQUE NOT NULL,
    password_hash VARCHAR(256) NOT NULL,
    created_at TIMESTAMP DEFAULT NOW()
);

-- accounts
CREATE TABLE accounts (
    account_id VARCHAR(64) PRIMARY KEY,
    user_id VARCHAR(64) REFERENCES users(user_id),
    name VARCHAR(64) NOT NULL,
    type VARCHAR(16) NOT NULL,
    tinkoff_token_encrypted TEXT,
    created_at TIMESTAMP DEFAULT NOW()
);

-- sessions
CREATE TABLE sessions (
    session_id VARCHAR(64) PRIMARY KEY,
    user_id VARCHAR(64) REFERENCES users(user_id),
    jwt_token VARCHAR(512) NOT NULL,
    expires_at TIMESTAMP NOT NULL,
    created_at TIMESTAMP DEFAULT NOW()
);
```

### Seed данные

```sql
-- Тестовый пользователь (password: test123)
INSERT INTO users VALUES ('user-test-001', 'testuser', 'test@example.com', 'hash:test123', NOW());

-- Sandbox аккаунт
INSERT INTO accounts VALUES ('acc-sandbox-001', 'user-test-001', 'Test Sandbox', 'SANDBOX', 'enc:fake-token', NOW());
```

## Примеры запросов

### Регистрация

```bash
curl -X POST http://arch.homework/auth/api/v1/auth/register \
  -H "Content-Type: application/json" \
  -d '{"username": "john", "email": "john@example.com", "password": "secret123"}'
```

### Логин

```bash
curl -X POST http://arch.homework/auth/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "testuser", "password": "test123"}'

# Response:
# {"session_token": "eyJ...", "user_id": "user-test-001", "message": "Login successful"}
```

### Получить аккаунты

```bash
curl http://arch.homework/auth/api/v1/accounts \
  -H "Authorization: Bearer eyJ..."
```

### Создать аккаунт

```bash
curl -X POST http://arch.homework/auth/api/v1/accounts \
  -H "Authorization: Bearer eyJ..." \
  -H "Content-Type: application/json" \
  -d '{"name": "My Sandbox", "type": "SANDBOX", "tinkoff_token": "fake-token"}'
```

## Postman

Коллекция для тестирования: `postman/auth-service.postman_collection.json`

```bash
# Импорт и запуск
newman run postman/auth-service.postman_collection.json
```

## Метрики

| Метрика | Тип | Описание |
|---------|-----|----------|
| `auth_uptime_seconds` | gauge | Время работы сервиса |
| `http_requests_total` | counter | Количество HTTP запросов |

## Переменные окружения

| Переменная | Описание | По умолчанию |
|------------|----------|--------------|
| `AUTH_DB_HOST` | Хост PostgreSQL | localhost |
| `AUTH_DB_PORT` | Порт PostgreSQL | 5432 |
| `AUTH_DB_NAME` | Имя базы данных | auth_db |
| `AUTH_DB_USER` | Пользователь БД | auth_user |
| `AUTH_DB_PASSWORD` | Пароль БД | **обязательно** |
| `JWT_SECRET` | Секрет для JWT | - |

## Взаимодействие с другими сервисами

```
Trading Service ──POST /api/v1/auth/validate──> Auth Service
                       {"token": "eyJ...", "type": "access"}
                <───── {"valid": true, "user_id": "...", "account_id": "..."}
```
