# TODO: Рефакторинг библиотеки cpp-http-server v2

> **Дата:** 2025-01-15  
> **Статус:** Согласовано  
> **Версия:** 2.0

---

## Содержание

1. [Обзор изменений](#обзор-изменений)
2. [IRequest — новый интерфейс](#irequest--новый-интерфейс)
3. [IResponse — новый интерфейс](#iresponse--новый-интерфейс)
4. [Path Parameters — первая итерация](#path-parameters--первая-итерация)
5. [Изменения в BoostBeastApplication](#изменения-в-boostbeastapplication)
6. [Идеи на будущее](#идеи-на-будущее)
7. [План реализации](#план-реализации)
8. [P1: Документация](#-p1-документация)
9. [P2: Новый функционал](#-p2-новый-функционал)
10. [Технический долг](#-технический-долг)
11. [Рефакторинг](#-рефакторинг)

---

## Обзор изменений

### Ключевые решения

| Тема | Было | Стало |
|------|------|-------|
| `getPath()` | Контракт не документирован | Возвращает БЕЗ query string (задокументировано) |
| `getFullPath()` | — | **Не добавляем** (не плодим баги) |
| `getParams()` | Неоднозначное имя | Переименован в `getQueryParams()` |
| Query params | Только `getParams()` | `getQueryParams()`, `getQueryParam(name)`, `setQueryParam()` |
| Path params | Нет поддержки | `getPathParam(index)`, `getPathPattern()`, `setPathPattern()` |
| Headers | Только `getHeaders()` | Добавлены `getHeader(name)`, `setHeader()`, `setHeaders()` |
| Body | Только getter | Добавлен `setBody()` |
| Bearer token | Дублирование в каждом handler | `getBearerToken()` в интерфейсе |
| Attributes | Нет | `setAttribute()`, `getAttribute()` для middleware |
| IResponse getters | Нет (только в SimpleResponse) | Добавлены в интерфейс |
| Convenience methods | Нет | `setResult(code, contentType, body)`, `isJson()` |

### Принципы проектирования

1. **Без внешних зависимостей в интерфейсе** — никаких `nlohmann::json` в сигнатурах
2. **Минимум дублирования** — но `getHeader(name)` допустим как оптимизация
3. **Документация каждого метода** — контракт, примеры использования
4. **Case-insensitive headers** — по HTTP стандарту (RFC 7230)

---

## IRequest — новый интерфейс

```cpp
#pragma once
#include <string>
#include <map>
#include <vector>
#include <optional>

/**
 * @file IRequest.hpp
 * @brief Интерфейс HTTP-запроса
 * @version 2.0
 */
struct IRequest {
    virtual ~IRequest() = default;

    // =========================================================================
    // PATH — работа с путём запроса
    // =========================================================================

    /**
     * @brief Получить путь запроса БЕЗ query string
     * @return Путь, например "/api/v1/orders"
     * 
     * @example
     *   Запрос: GET /api/v1/orders?status=active
     *   getPath() → "/api/v1/orders"
     * 
     * @contract Всегда возвращает путь без query string.
     */
    virtual std::string getPath() const = 0;

    /**
     * @brief Получить сегменты пути
     * @return Вектор сегментов (без пустых элементов)
     * 
     * @example
     *   Путь: /api/v1/orders/ord-123
     *   getPathSegments() → ["api", "v1", "orders", "ord-123"]
     * 
     * @example
     *   Путь: /
     *   getPathSegments() → []
     */
    virtual std::vector<std::string> getPathSegments() const = 0;

    // =========================================================================
    // PATH PARAMETERS — параметры из URL паттерна
    // =========================================================================

    /**
     * @brief Получить паттерн, по которому был найден handler
     * @return Паттерн или пустая строка если не установлен
     * 
     * @example
     *   Паттерн: /api/v1/orders/*
     *   getPathPattern() → "/api/v1/orders/*"
     * 
     * @note Устанавливается в BoostBeastApplication::handleRequest() 
     *       после матчинга маршрута.
     */
    virtual std::string getPathPattern() const = 0;

    /**
     * @brief Установить паттерн маршрута
     * @param pattern Паттерн с wildcards
     * 
     * @note Вызывается в BoostBeastApplication::handleRequest()
     *       после успешного матчинга в findHandler().
     */
    virtual void setPathPattern(const std::string& pattern) = 0;

    /**
     * @brief Получить path parameter по индексу wildcard
     * @param index Индекс wildcard в паттерне (начиная с 0)
     * @return Значение параметра или nullopt если индекс вне диапазона
     * 
     * @example
     *   Паттерн: /api/v1/orders/*
     *   Путь:    /api/v1/orders/ord-123
     *   getPathParam(0) → "ord-123"
     * 
     * @example
     *   Паттерн: /api/v1/orders/*/items/*
     *   Путь:    /api/v1/orders/ord-123/items/item-456
     *   getPathParam(0) → "ord-123"
     *   getPathParam(1) → "item-456"
     *   getPathParam(2) → nullopt
     * 
     * @note Вычисляется на основе getPath() и getPathPattern().
     *       Сравниваются сегменты: где pattern[i] == "*", 
     *       берётся path[i] как значение параметра.
     */
    virtual std::optional<std::string> getPathParam(size_t index) const = 0;

    // =========================================================================
    // QUERY PARAMETERS — параметры из query string
    // =========================================================================

    /**
     * @brief Получить все query parameters
     * @return Map имя → значение
     * 
     * @example
     *   Запрос: GET /orders?status=active&limit=10
     *   getQueryParams() → {"status": "active", "limit": "10"}
     * 
     * @note Переименован из getParams() для ясности.
     */
    virtual std::map<std::string, std::string> getQueryParams() const = 0;

    /**
     * @brief Получить query parameter по имени
     * @param name Имя параметра
     * @return Значение или nullopt если не найден
     * 
     * @example
     *   Запрос: GET /orders?status=active
     *   getQueryParam("status") → "active"
     *   getQueryParam("limit") → nullopt
     */
    virtual std::optional<std::string> getQueryParam(const std::string& name) const = 0;

    /**
     * @brief Установить query parameter
     * @param name Имя параметра
     * @param value Значение параметра
     */
    virtual void setQueryParam(const std::string& name, const std::string& value) = 0;

    // =========================================================================
    // HEADERS — HTTP заголовки
    // =========================================================================

    /**
     * @brief Получить все HTTP заголовки
     * @return Map имя → значение
     */
    virtual std::map<std::string, std::string> getHeaders() const = 0;

    /**
     * @brief Получить значение заголовка по имени
     * @param name Имя заголовка
     * @return Значение или nullopt если не найден
     * 
     * @note Case-insensitive по HTTP стандарту (RFC 7230).
     *       getHeader("Content-Type") == getHeader("content-type")
     * 
     * @example
     *   getHeader("Content-Type") → "application/json"
     *   getHeader("Authorization") → "Bearer eyJ..."
     */
    virtual std::optional<std::string> getHeader(const std::string& name) const = 0;

    /**
     * @brief Установить заголовок
     * @param name Имя заголовка
     * @param value Значение заголовка
     */
    virtual void setHeader(const std::string& name, const std::string& value) = 0;

    /**
     * @brief Установить несколько заголовков
     * @param headers Map имя → значение
     * 
     * @note Существующие заголовки с такими же именами перезаписываются.
     */
    virtual void setHeaders(const std::map<std::string, std::string>& headers) = 0;

    // =========================================================================
    // BODY — тело запроса
    // =========================================================================

    /**
     * @brief Получить тело запроса
     * @return Тело запроса как строка
     */
    virtual std::string getBody() const = 0;

    /**
     * @brief Установить тело запроса
     * @param body Тело запроса
     */
    virtual void setBody(const std::string& body) = 0;

    // =========================================================================
    // METHOD — HTTP метод
    // =========================================================================

    /**
     * @brief Получить HTTP метод
     * @return Метод в верхнем регистре (GET, POST, PUT, DELETE, PATCH, etc.)
     */
    virtual std::string getMethod() const = 0;

    // =========================================================================
    // CONNECTION INFO — информация о соединении
    // =========================================================================

    /**
     * @brief Получить IP-адрес
     * @return IP клиента (для входящих) или целевой IP (для исходящих)
     */
    virtual std::string getIp() const = 0;

    /**
     * @brief Получить порт
     * @return Порт (80 по умолчанию для входящих, целевой для исходящих)
     */
    virtual int getPort() const = 0;

    // =========================================================================
    // CONVENIENCE METHODS — удобные методы
    // =========================================================================

    /**
     * @brief Извлечь Bearer token из заголовка Authorization
     * @return Token без префикса "Bearer " или nullopt
     * 
     * @example
     *   Header: Authorization: Bearer eyJ...
     *   getBearerToken() → "eyJ..."
     * 
     *   Header: Authorization: Basic abc123
     *   getBearerToken() → nullopt
     * 
     *   Header отсутствует
     *   getBearerToken() → nullopt
     * 
     * @rationale Используется в 90% handlers для аутентификации.
     *            Устраняет дублирование кода извлечения токена.
     */
    virtual std::optional<std::string> getBearerToken() const = 0;

    /**
     * @brief Проверить, является ли Content-Type JSON
     * @return true если Content-Type содержит "json"
     * 
     * @example
     *   Content-Type: application/json → true
     *   Content-Type: application/json; charset=utf-8 → true
     *   Content-Type: text/plain → false
     *   Content-Type отсутствует → false
     */
    virtual bool isJson() const = 0;

    /**
     * @brief Получить Content-Type
     * @return Значение Content-Type или пустая строка если не установлен
     */
    virtual std::string getContentType() const = 0;

    // =========================================================================
    // ATTRIBUTES — передача данных между middleware/handlers
    // =========================================================================

    /**
     * @brief Установить атрибут запроса
     * @param name Имя атрибута
     * @param value Значение атрибута
     * 
     * @note Атрибуты хранятся в объекте Request (map<string, string>)
     *       и используются для передачи данных между middleware 
     *       и handlers в рамках одного запроса.
     * 
     * @example
     *   // В AuthMiddleware после валидации токена:
     *   req.setAttribute("user_id", "user-123");
     *   req.setAttribute("account_id", "acc-456");
     *   req.setAttribute("account_type", "sandbox");
     */
    virtual void setAttribute(const std::string& name, const std::string& value) = 0;

    /**
     * @brief Получить атрибут запроса
     * @param name Имя атрибута
     * @return Значение или nullopt если не установлен
     * 
     * @example
     *   // В OrderHandler:
     *   auto userId = req.getAttribute("user_id");
     *   if (!userId) {
     *       // Ошибка: middleware не установил user_id
     *   }
     */
    virtual std::optional<std::string> getAttribute(const std::string& name) const = 0;
};
```

---

## IResponse — новый интерфейс

```cpp
#pragma once
#include <string>
#include <map>
#include <optional>

/**
 * @file IResponse.hpp
 * @brief Интерфейс HTTP-ответа
 * @version 2.0
 */
struct IResponse {
    virtual ~IResponse() = default;

    // =========================================================================
    // SETTERS — установка данных ответа
    // =========================================================================

    /**
     * @brief Установить HTTP статус код
     * @param code Код статуса (200, 201, 400, 401, 404, 500, etc.)
     */
    virtual void setStatus(int code) = 0;

    /**
     * @brief Установить тело ответа
     * @param body Тело ответа
     */
    virtual void setBody(const std::string& body) = 0;

    /**
     * @brief Установить заголовок ответа
     * @param name Имя заголовка
     * @param value Значение заголовка
     */
    virtual void setHeader(const std::string& name, const std::string& value) = 0;

    // =========================================================================
    // GETTERS — получение данных ответа
    // =========================================================================

    /**
     * @brief Получить HTTP статус код
     * @return Код статуса
     * 
     * @note Используется для logging middleware, тестирования.
     */
    virtual int getStatus() const = 0;

    /**
     * @brief Получить тело ответа
     * @return Тело ответа
     */
    virtual std::string getBody() const = 0;

    /**
     * @brief Получить все заголовки ответа
     * @return Map имя → значение
     */
    virtual std::map<std::string, std::string> getHeaders() const = 0;

    /**
     * @brief Получить заголовок по имени
     * @param name Имя заголовка
     * @return Значение или nullopt если не установлен
     * 
     * @note Case-insensitive по HTTP стандарту.
     */
    virtual std::optional<std::string> getHeader(const std::string& name) const = 0;

    // =========================================================================
    // CONVENIENCE METHODS — удобные методы
    // =========================================================================

    /**
     * @brief Установить полный результат ответа
     * @param code HTTP статус код
     * @param contentType Значение Content-Type
     * @param body Тело ответа
     * 
     * @note Эквивалентно:
     *   setStatus(code);
     *   setHeader("Content-Type", contentType);
     *   setBody(body);
     * 
     * @example
     *   // JSON успешный ответ
     *   res.setResult(200, "application/json", R"({"status": "ok"})");
     * 
     *   // JSON ошибка
     *   res.setResult(404, "application/json", R"({"error": "Not found"})");
     * 
     *   // Plain text
     *   res.setResult(200, "text/plain", "Hello, World!");
     * 
     * @rationale Устраняет повторяющийся паттерн в каждом handler:
     *   res.setStatus(200);
     *   res.setHeader("Content-Type", "application/json");
     *   res.setBody(json.dump());
     */
    virtual void setResult(int code, 
                           const std::string& contentType, 
                           const std::string& body) = 0;
};
```

---

## Path Parameters — первая итерация

### Поддерживаемый синтаксис

В первой итерации поддерживаем **только anonymous wildcards** (`*`):

| Синтаксис | Пример паттерна | Пример URL | Результат |
|-----------|-----------------|------------|-----------|
| `*` | `/orders/*` | `/orders/ord-123` | `getPathParam(0)` → "ord-123" |
| `*` | `/orders/*/items/*` | `/orders/ord-123/items/item-456` | `getPathParam(0)` → "ord-123", `getPathParam(1)` → "item-456" |

### Алгоритм вычисления getPathParam(index)

```cpp
std::optional<std::string> BeastRequestAdapter::getPathParam(size_t index) const {
    std::string path = getPath();
    std::string pattern = getPathPattern();
    
    if (pattern.empty()) {
        return std::nullopt;
    }
    
    auto pathSegments = split(path, '/');
    auto patternSegments = split(pattern, '/');
    
    size_t wildcardIndex = 0;
    for (size_t i = 0; i < patternSegments.size() && i < pathSegments.size(); ++i) {
        if (patternSegments[i] == "*") {
            if (wildcardIndex == index) {
                return pathSegments[i];
            }
            ++wildcardIndex;
        }
    }
    
    return std::nullopt;
}
```

---

## Изменения в BoostBeastApplication

### 1. Константа-разделитель

```cpp
// В BoostBeastApplication.cpp (или в отдельном header)
namespace {
    // Разделитель между методом и паттерном в ключе хэндлера.
    // 
    // Примеры ключей:
    //   "GET:/api/v1/orders"      - exact match
    //   "GET:/api/v1/orders/*"    - wildcard match
    //   "POST:/api/v1/auth/login" - exact match
    //
    // ВАЖНО: При переходе на Express.js стиль path parameters (:orderId)
    // символ ':' создаст неоднозначность в ключе: "GET:/api/v1/orders/:orderId"
    // Первый ':' — разделитель, второй ':' — начало параметра.
    // Текущий код find(':') найдёт первый, что корректно.
    // Но для читаемости рекомендуется заменить на '|' или '#' при рефакторинге.
    constexpr char HANDLER_KEY_DELIMITER = ':';
}
```

### 2. Структура HandlerMatch

```cpp
// В BoostBeastApplication.hpp
struct HandlerMatch {
    std::shared_ptr<IHttpHandler> handler;
    std::string pattern;  // паттерн, по которому найден handler
};
```

### 3. Обновлённый findHandler()

```cpp
// В BoostBeastApplication.hpp
protected:
    std::optional<HandlerMatch> findHandler(
        const std::string& method, 
        const std::string& path);
```

```cpp
// В BoostBeastApplication.cpp
std::optional<HandlerMatch> BoostBeastApplication::findHandler(
    const std::string& method,
    const std::string& path)
{
    // 1. Точное совпадение (для маршрутов без wildcards)
    std::string exactKey = getHandlerKey(method, path);
    auto it = handlers_.find(exactKey);
    if (it != handlers_.end())
    {
        // Для exact match паттерн совпадает с путём
        return HandlerMatch{it->second, path};
    }

    // 2. Поиск по wildcards
    for (const auto& [key, handler] : handlers_)
    {
        size_t delimPos = key.find(HANDLER_KEY_DELIMITER);
        if (delimPos == std::string::npos)
            continue;

        std::string handlerMethod = key.substr(0, delimPos);
        std::string handlerPattern = key.substr(delimPos + 1);

        if (handlerMethod == method && RouteMatcher::matches(handlerPattern, path))
        {
            return HandlerMatch{handler, handlerPattern};
        }
    }

    return std::nullopt;
}
```

### 4. Обновлённый handleRequest()

```cpp
void BoostBeastApplication::handleRequest(IRequest& req, IResponse& res)
{
    std::string path = req.getPath();
    std::string method = req.getMethod();

    std::cout << "[BoostBeastApplication] " << method << " " << path
              << " from " << req.getIp() << std::endl;

    auto match = findHandler(method, path);

    if (match)
    {
        try
        {
            // Устанавливаем паттерн для вычисления path parameters
            req.setPathPattern(match->pattern);
            match->handler->handle(req, res);
        }
        catch (const std::exception& e)
        {
            std::cerr << "[BoostBeastApplication] Handler error: " << e.what() << std::endl;
            res.setResult(500, "application/json", R"({"error": "Internal server error"})");
        }
    }
    else
    {
        std::cout << "[BoostBeastApplication] No handler found" << std::endl;
        res.setResult(404, "application/json", R"({"error": "Not found"})");
    }
}
```

### 5. Обновлённый getHandlerKey()

```cpp
std::string BoostBeastApplication::getHandlerKey(
    const std::string& method, 
    const std::string& pattern) const
{
    return method + HANDLER_KEY_DELIMITER + pattern;
}
```

---

## Идеи на будущее

### 1. Named path parameters (Express.js стиль)

```
Статус: НЕ реализуем в первой итерации
Причина: Требует значительного рефакторинга существующего кода

Синтаксис:
  Паттерн: /api/v1/orders/:orderId/items/:itemId
  
Использование:
  getPathParam("orderId") → "ord-123"
  getPathParam("itemId") → "item-456"
  
Требуемые изменения:
  1. Обновить RouteMatcher для поддержки :name синтаксиса
  2. Добавить getPathParam(const std::string& name) в IRequest
  3. Изменить разделитель в ключе хэндлера с ':' на '|' или '#'
  4. Обновить все существующие регистрации маршрутов
```

### 2. Trie-based routing (древовидная структура)

```
Идея: использовать trie (префиксное дерево) для эффективного матчинга.

Текущий подход: O(n) — перебор всех зарегистрированных маршрутов
Trie подход: O(k) — где k = количество сегментов пути

Структура дерева:
  root
    └── api
          └── v1
                ├── orders
                │     └── * (wildcard handler)
                └── users
                      └── * (handler)

Преимущества:
- Эффективный lookup
- Естественная поддержка wildcard и named parameters
- Возможность добавить приоритеты (exact match > wildcard)
```

### 3. Кодогенерация DI из config.json

```
Идея: генерировать C++ код регистрации хэндлеров из конфигурации.

Причина невозможности runtime lookup в Boost.DI:
  - named() в Boost.DI работает через compile-time типы
  - Lookup по std::string из HTTP-запроса невозможен
  
Альтернатива:
  1. Читаем config.json на этапе сборки (CMake/скрипт)
  2. Генерируем C++ код с регистрацией
  3. Компилируем сгенерированный код

Статус: Отложено, текущий подход с handlers_ map работает.
```

---

## План реализации

### Фаза 1: Базовые изменения интерфейсов (P0)

| Задача | Оценка | Описание |
|--------|--------|----------|
| Обновить IRequest | 2h | Добавить все новые методы |
| Обновить IResponse | 1h | Добавить геттеры и setResult() |
| Обновить BeastRequestAdapter | 3h | Реализовать новые методы + case-insensitive headers |
| Обновить BeastResponseAdapter | 1h | Реализовать новые методы |
| Обновить SimpleRequest | 1h | Реализовать новые методы |
| Обновить SimpleResponse | 0.5h | Добавить setResult() |
| Unit-тесты | 3h | Тесты для всех новых методов |

**Итого Фаза 1:** ~11.5 часов

### Фаза 2: Path Parameters (P0)

| Задача | Оценка | Описание |
|--------|--------|----------|
| HandlerMatch структура | 0.5h | Создать структуру |
| Обновить findHandler() | 1h | Возвращать HandlerMatch |
| Обновить handleRequest() | 0.5h | Использовать setPathPattern() |
| Реализовать getPathParam(index) | 1h | В BeastRequestAdapter |
| Константа HANDLER_KEY_DELIMITER | 0.5h | Вынести разделитель |
| Unit-тесты | 2h | Тесты path parameters |

**Итого Фаза 2:** ~5.5 часов

### Фаза 3: Рефакторинг существующего кода (P1)

| Задача | Оценка | Описание |
|--------|--------|----------|
| Переименовать getParams() → getQueryParams() | 1h | Во всех handlers |
| Использовать getBearerToken() | 1h | Заменить дублирующийся код |
| Использовать setResult() | 1h | Заменить setStatus+setHeader+setBody |

**Итого Фаза 3:** ~3 часа

### Общая оценка: ~20 часов

---

## Чеклист для code review

### IRequest
- [ ] getPath() возвращает путь БЕЗ query string
- [ ] getHeader(name) — case-insensitive
- [ ] getBearerToken() — корректно обрабатывает отсутствие заголовка
- [ ] isJson() — использует contains("json")
- [ ] getPathParam(index) — корректно вычисляет по паттерну

### IResponse  
- [ ] setResult() — устанавливает status, header, body
- [ ] getHeader(name) — case-insensitive

### BoostBeastApplication
- [ ] findHandler() возвращает HandlerMatch
- [ ] handleRequest() вызывает setPathPattern()
- [ ] HANDLER_KEY_DELIMITER вынесен в константу с комментарием

---

## 🟡 P1: Документация

### Структура документации

```
cpp-http-server/
├── README.md           # Главный файл, обзор проекта
├── CHANGELOG.md        # История изменений по версиям
├── TODO.md             # Тактические задачи, тех.долг
├── LICENSE             # Лицензия
├── .gitignore
├── docs/
│   ├── api.md          # Документация API (IRequest, IResponse)
│   ├── routing.md      # Документация роутинга
│   └── deployment.md   # Инструкция по деплою
```

### Открытые вопросы

| Вопрос | Варианты |
|--------|----------|
| Какую лицензию выбираем? | MIT? Apache 2.0? |
| Когда делаем документацию? | После реализации Фазы 1-2 или параллельно? |

---

## 🟢 P2: Новый функционал

### 1. HealthCheckHandler

**Описание:** Дефолтный health-check хэндлер, используется во всех приложениях.

**Конфигурация:** Настройки из config.json → HealthCheckHandlerSettings → HealthCheckHandler

**Открытые вопросы:**

| Вопрос | Варианты |
|--------|----------|
| Формат ответа | A) Минимальный: `{"status": "ok"}` |
| | B) С метаданными: `{"status": "ok", "service": "...", "version": "..."}` |
| | C) Оба варианта (настраивается) |

### 2. MetricsHandler

**Описание:** Дефолтный /metrics хэндлер для сбора метрик Prometheus.

**Формат:** `text/plain` с метриками в Prometheus-совместимом формате.

**Статус:** Не в приоритете, требует проработки.

---

## 📦 Технический долг

### 1. "Умный" DI — кодогенерация из di.json

**Идея:** 
1. В `di.json` указываем зависимости и хэндлеры
2. При компиляции генерируется класс `DiCommand` с методом `void exec()`
3. В методе `createInjection()` приложения вызываем `diCommand.exec()`

**Включает:**
- Автоматическую регистрацию хэндлеров
- Привязку HttpHandlerKey (method + pattern) к хэндлерам

**Открытые вопросы:**

| Вопрос | Варианты |
|--------|----------|
| Когда генерировать код? | CMake custom command? Отдельный скрипт? |
| На чём писать генератор? | Python? C++? |
| Область применения | Только библиотека или все сервисы? |

### 2. HttpHandlerKey структура

**Идея:** Заменить строковый ключ `"GET:/api/v1/orders/*"` на структуру:

```cpp
struct HttpHandlerKey {
    std::string method;   // GET, POST, etc.
    std::string pattern;  // /api/v1/orders/*
};
```

**Открытые вопросы:**

| Вопрос | Комментарий |
|--------|-------------|
| Заменяем строковый ключ? | Потребует изменить `handlers_` map |
| Когда делаем? | Вместе с кодогенерацией или отдельно? |

---

## 🔧 Рефакторинг

### Разделение hpp/cpp

**Текущее состояние:** Много логики в header файлах (header-only подход).

**Предложение:**
- hpp: только интерфейсы, шаблоны, inline функции
- cpp: вся имплементация

**Преимущества:**
- Ускорит компиляцию
- Улучшит читаемость
- Упростит отладку

**Открытые вопросы:**

| Вопрос | Варианты |
|--------|----------|
| Когда делаем? | После функциональных изменений или параллельно? |
| Какие файлы в первую очередь? | BeastRequestAdapter, BeastResponseAdapter, BoostBeastApplication? |

---

*Документ создан: 2025-01-15*
*Последнее обновление: 2025-01-15*
