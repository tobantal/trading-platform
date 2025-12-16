# 🚀 Trading Platform — HelloWorld

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Docker](https://img.shields.io/badge/Docker-Compose-2496ED?logo=docker&logoColor=white)](https://docs.docker.com/compose/)
[![Boost](https://img.shields.io/badge/Boost-Beast%20%7C%20DI-orange)](https://www.boost.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

> Микросервис на C++17 с Boost.Beast, Boost.DI и гексагональной архитектурой.  
> Демонстрационный проект для курса OTUS "Microservice Architecture".

---

## 📋 Содержание

- [Быстрый старт](#-быстрый-старт)
- [Архитектура](#-архитектура)
- [Структура проекта](#-структура-проекта)
- [API Endpoints](#-api-endpoints)
- [Инфраструктура](#-инфраструктура)
- [Конфигурация](#-конфигурация)
- [Разработка](#-разработка)
- [Используемые библиотеки](#-используемые-библиотеки)

---

## 🚀 Быстрый старт

### Запуск через Docker Compose (рекомендуется)

```bash
# Клонируем репозиторий
git clone https://github.com/tobantal/trading-helloworld.git
cd trading-helloworld

# Запускаем все сервисы
docker compose up -d

# Проверяем статус
docker ps
```

### Проверка работоспособности

```bash
# Health check
curl http://localhost:8080/api/v1/health

# Metrics
curl http://localhost:8080/metrics

# Echo
curl "http://localhost:8080/echo?message=hello"
```

### Доступные URL

| Сервис | URL | Описание |
|--------|-----|----------|
| 🌐 Web UI | http://localhost:3001 | Интерактивная страница |
| 🔧 Backend API | http://localhost:8080 | REST API напрямую |
| 📊 Prometheus | http://localhost:9090 | Мониторинг метрик |
| 🐘 PostgreSQL | localhost:5432 | База данных |

---

## 🏗 Архитектура

### Гексагональная архитектура (Ports & Adapters)

```
┌─────────────────────────────────────────────────────────────┐
│                        NGINX (:3001)                        │
│                     Reverse Proxy + Static                  │
└─────────────────────┬───────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────┐
│                    BACKEND (:8080)                          │
│  ┌───────────────────────────────────────────────────────┐  │
│  │                   HelloWorldApp                       │  │
│  │              (extends BoostBeastApplication)          │  │
│  └───────────────────────┬───────────────────────────────┘  │
│                          │                                  │
│  ┌───────────────────────▼───────────────────────────────┐  │
│  │              Boost.DI Container                       │  │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐     │  │
│  │  │HealthCheck │ │  Metrics    │ │    Echo     │     │  │
│  │  │  Handler   │ │  Handler    │ │   Handler   │     │  │
│  │  └─────────────┘ └─────────────┘ └─────────────┘     │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                      │                    │
        ┌─────────────▼──────┐    ┌───────▼────────┐
        │  PostgreSQL (:5432)│    │ Prometheus     │
        │     Database       │    │    (:9090)     │
        └────────────────────┘    └────────────────┘
```

### Поток запроса

```
Browser → Nginx → Backend → Handler → Response
   │                           │
   │    ┌──────────────────────┘
   │    ▼
   │  1. Nginx получает запрос на :3001
   │  2. Статика (/) → отдаёт index.html
   │  3. API (/api/*, /metrics, /echo) → проксирует на backend:8080
   │  4. Backend находит Handler по route
   │  5. Handler обрабатывает и возвращает Response
   │
   └──── Response возвращается клиенту
```

---

## 📁 Структура проекта

```
hello-world/
├── 📄 CMakeLists.txt           # Сборка + FetchContent зависимостей
├── 📄 config.json              # Конфигурация приложения
├── 🐳 Dockerfile               # Multi-stage сборка
├── 🐳 docker-compose.yml       # Оркестрация сервисов
│
├── 📂 config/
│   ├── nginx.conf              # Конфигурация reverse proxy
│   └── prometheus.yml          # Конфигурация мониторинга
│
├── 📂 html/
│   └── index.html              # Web UI для тестирования
│
├── 📂 include/
│   ├── HelloWorldApp.hpp       # Application class
│   └── handlers/
│       ├── HealthCheckHandler.hpp
│       ├── MetricsHandler.hpp
│       └── EchoHandler.hpp
│
└── 📂 src/
    ├── main.cpp                # Entry point
    ├── HelloWorldApp.cpp       # DI configuration
    └── handlers/
        ├── HealthCheckHandler.cpp
        ├── MetricsHandler.cpp
        └── EchoHandler.cpp
```

---

## 📡 API Endpoints

### `GET /api/v1/health`

Проверка состояния сервиса.

**Response:**
```json
{
  "status": "ok",
  "timestamp": "2025-12-15T21:40:35Z",
  "services": {
    "http_server": "ready",
    "cache": "ready",
    "postgres": "pending"
  }
}
```

### `GET /metrics`

Метрики в формате Prometheus.

**Response:**
```
# HELP http_requests_total Total HTTP requests
# TYPE http_requests_total counter
http_requests_total 42

# HELP cache_hits_total Cache hits
# TYPE cache_hits_total counter
cache_hits_total 10

# HELP cache_misses_total Cache misses
# TYPE cache_misses_total counter
cache_misses_total 2
```

### `GET /echo?message={text}`

Echo-сервис для тестирования.

**Request:**
```bash
curl "http://localhost:8080/echo?message=hello"
```

**Response:**
```json
{
  "message": "hello",
  "timestamp": "2025-12-15T21:40:35Z"
}
```

---

## 🔧 Инфраструктура

### Docker Compose сервисы

| Контейнер | Образ | Порт | Назначение |
|-----------|-------|------|------------|
| `trading-helloworld` | Custom (C++) | 8080 | Backend API |
| `trading-nginx` | nginx:alpine | 3001 | Reverse proxy + статика |
| `trading-postgres` | postgres:15-alpine | 5432 | База данных |
| `trading-prometheus` | prom/prometheus | 9090 | Мониторинг |

### Nginx маршрутизация

```nginx
location /           → index.html (статика)
location /api/*      → backend:8080 (proxy)
location /metrics    → backend:8080 (proxy)
location /echo       → backend:8080 (proxy)
location /prometheus → prometheus:9090 (proxy)
```

### Health Checks

Все сервисы имеют health checks для правильного порядка запуска:

- **PostgreSQL**: `pg_isready -U trader -d trading`
- **Prometheus**: `wget -q --spider http://localhost:9090/-/healthy`
- **Backend**: `curl -f http://localhost:8080/api/v1/health`

---

## ⚙️ Конфигурация

### config.json

```json
{
  "server": {
    "host": "0.0.0.0",
    "port": 8080
  },
  "database": {
    "host": "postgres",
    "port": 5432,
    "name": "trading",
    "user": "trader",
    "password": "password"
  },
  "cache": {
    "enabled": true,
    "capacity": 1000,
    "policy": "lru"
  },
  "logging": {
    "level": "info",
    "format": "json"
  }
}
```

### Переменные окружения

| Переменная | Значение | Описание |
|------------|----------|----------|
| `LOG_LEVEL` | info | Уровень логирования |
| `POSTGRES_DB` | trading | Имя базы данных |
| `POSTGRES_USER` | trader | Пользователь БД |
| `POSTGRES_PASSWORD` | password | Пароль БД |

---

## 💻 Разработка

### Локальная сборка (без Docker)

```bash
# Создаём build директорию
mkdir -p build && cd build

# Конфигурируем (FetchContent загрузит зависимости)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Собираем
cmake --build . --config Release

# Запускаем (нужен config.json в текущей директории)
cp ../config.json .
./trading-helloworld
```

### Пересборка Docker образа

```bash
# Полная пересборка
docker compose down
docker compose up -d --build

# Только backend
docker compose build backend
docker compose up -d backend
```

### Просмотр логов

```bash
# Все сервисы
docker compose logs -f

# Только backend
docker logs -f trading-helloworld

# Только nginx
docker logs -f trading-nginx
```

### Остановка

```bash
# Остановить все
docker compose down

# Остановить и удалить volumes
docker compose down -v
```

---

## 📚 Используемые библиотеки

| Библиотека | Версия | Назначение |
|------------|--------|------------|
| [cpp-http-server](https://github.com/tobantal/cpp-http-server) | v0.0.5 | HTTP сервер на Boost.Beast |
| [cpp-cache](https://github.com/tobantal/cpp-cache) | v0.0.1 | LRU кэш |
| [Boost.Beast](https://www.boost.org/doc/libs/release/libs/beast/) | 1.83.0 | HTTP/WebSocket |
| [Boost.DI](https://github.com/boost-ext/di) | v1.3.0 | Dependency Injection |
| [nlohmann/json](https://github.com/nlohmann/json) | v3.11.3 | JSON парсинг |

---

## 🎯 Acceptance Criteria

- [x] CMake FetchContent загружает cpp-cache и cpp-http-server
- [x] HelloWorldApp наследует BoostBeastApplication
- [x] Handlers регистрируются через Boost.DI
- [x] `GET /api/v1/health` возвращает 200 + JSON
- [x] `GET /metrics` возвращает Prometheus format
- [x] `GET /echo?message=...` возвращает JSON с timestamp
- [x] `docker compose up` запускает все сервисы
- [x] Nginx проксирует API запросы на backend
- [x] Prometheus собирает метрики с backend

---

## 📄 Лицензия

MIT License. См. [LICENSE](LICENSE).

---

<p align="center">
  <b>Trading Platform</b> — OTUS Microservice Architecture Course Project
</p>
