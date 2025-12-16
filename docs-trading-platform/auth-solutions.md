# 🔐 Готовые Docker решения для авторизации: Обзор и рекомендация

## 💡 Задача
Найти готовый Docker образ для авторизации с поддержкой JWT, который:
- Подключается за **< 2 часа**
- Требует **минимальной конфигурации**
- Рабочий для **MVP и Education**
- Можно заменить на свой решение для Production

---

## 📊 Сравнительная таблица решений

| Решение | Время подключения | Сложность | Размер образа | Требования | JWT | OAuth2 | Рекомендация |
|---------|-------------------|-----------|---------------|-----------|-----|--------|--------------|
| **fake-jwt-server** | **30 мин** ⚡ | Очень просто | ~20MB | None | ✅ | ❌ | 🌟 **ИДЕАЛ** |
| **loginsrv** | 45 мин | Просто | ~50MB | PostgreSQL | ✅ | ✅ | ✅ **ХОРОШО** |
| **Keycloak (dev)** | 1.5 часа | Средне | ~800MB | PostgreSQL | ✅ | ✅ | ⚠️ Тяжело |
| **Authentik** | 2 часа | Средне | ~500MB | PostgreSQL | ✅ | ✅ | ⚠️ Требует ресурсов |
| **Authelia** | 1.5 часа | Средне | ~15MB | Redis | ✅ | ✅ | ✅ Легко |
| **ZITADEL** | 1 час | Средне | ~200MB | PostgreSQL | ✅ | ✅ | ✅ Хороший выбор |

---

## 🌟 РЕКОМЕНДАЦИЯ: **fake-jwt-server**

### Почему именно это?

```
✅ Супер простой (буквально 1 endpoint)
✅ Минимум конфигурации (просто env variables)
✅ Очень быстрый (не требует базы)
✅ Идеален для разработки и тестирования
✅ Можно запустить за 30 минут
❌ Не для production (нет управления пользователями)
```

---

## 🚀 Вариант 1: fake-jwt-server (РЕКОМЕНДУЕТСЯ для MVP/Education)

### Что это?
Простой мок Identity Provider (IDP) для выдачи JWT токенов. Создан компанией Stackit для локальной разработки.

### Минимум конфигурации

```bash
# Docker Compose
services:
  fake-jwt-server:
    image: ghcr.io/stackitcloud/fake-jwt-server:latest
    ports:
      - "8008:8008"
    environment:
      - PORT=8008
      - SUBJECT_ID_PREFIX=user
```

### Как использовать

```bash
# Получить JWT токен
curl -X POST http://localhost:8008/token \
  -H "Content-Type: application/json" \
  -d '{"sub":"user123","name":"Test User"}'

# Ответ:
# {
#   "access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
#   "token_type": "Bearer",
#   "expires_in": 3600
# }

# Проверить публичный ключ (для верификации)
curl http://localhost:8008/.well-known/jwks.json
```

### Интеграция с backend

```cpp
// В C++ коде просто проверяем токен
#include <jwt/jwt.hpp>

bool verify_token(const std::string& token) {
    // Получаем публичный ключ с http://fake-jwt-server:8008/.well-known/jwks.json
    // Проверяем подпись
    auto decoded = jwt::decode(token, secret_key);
    return decoded.has_claim("sub");
}
```

### ⏱️ Время на подключение
- **Добавить в docker-compose**: 5 мин
- **Создать endpoint `/auth/login` который вызывает fake-jwt-server**: 10 мин
- **Проверить что работает**: 10 мин
- **Итого**: ~30 минут ✅

### docker-compose.yml фрагмент

```yaml
services:
  fake-jwt-server:
    image: ghcr.io/stackitcloud/fake-jwt-server:latest
    container_name: trading-platform-fake-jwt
    ports:
      - "8008:8008"
    environment:
      - PORT=8008
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8008/.well-known/jwks.json"]
      interval: 10s
      timeout: 5s
      retries: 3
    networks:
      - trading-network
    restart: unless-stopped
```

---

## ✅ Вариант 2: loginsrv (Альтернатива)

### Что это?
Легковесный сервер аутентификации с поддержкой OAuth2 и JWT. Написан на Go.

### Плюсы
- Небольшой размер
- Поддержка OAuth2 провайдеров (Google, GitHub и т.д.)
- Встроенная поддержка LDAP

### Минусы
- Требует настройки конфигурационного файла
- Может требовать PostgreSQL для хранения сессий

### Быстрый старт

```yaml
services:
  loginsrv:
    image: tarentino/loginsrv:latest
    ports:
      - "8080:8080"
    environment:
      - LOGINSRV_JWT_SECRET=your-secret-key-change-in-prod
      - LOGINSRV_BACKEND=simple
      - LOGINSRV_SIMPLE_USERS=user:password
    networks:
      - trading-network
```

### ⏱️ Время на подключение
- Добавить в docker-compose: 5 мин
- Настроить конфигурацию: 20 мин
- Протестировать: 15 мин
- **Итого**: ~45 минут

---

## 🔧 Вариант 3: Keycloak (Если нужна функциональность)

### Что это?
Полнофункциональный сервер управления идентичностью с поддержкой OAuth2, OIDC, SAML.

### Плюсы
- Поддержка многих протоколов
- Admin console с интерфейсом
- Встроенная поддержка социальных логинов

### Минусы
- **ТЯЖЕЛЫЙ**: 800MB образ, требует 512MB+ RAM
- Медленнее других вариантов
- Сложнее конфигурация
- Требует PostgreSQL

### Быстрый старт

```yaml
services:
  postgres:
    image: postgres:15-alpine
    environment:
      POSTGRES_DB: keycloak
      POSTGRES_USER: keycloak
      POSTGRES_PASSWORD: keycloak

  keycloak:
    image: quay.io/keycloak/keycloak:latest
    ports:
      - "8080:8080"
    environment:
      KC_BOOTSTRAP_ADMIN_USERNAME: admin
      KC_BOOTSTRAP_ADMIN_PASSWORD: admin
      KC_DB: postgres
      KC_DB_URL: jdbc:postgresql://postgres:5432/keycloak
      KC_DB_USERNAME: keycloak
      KC_DB_PASSWORD: keycloak
    command: start-dev
    depends_on:
      - postgres
```

### ⏱️ Время на подключение
- Добавить в docker-compose: 10 мин
- Настроить Realm + Client: 30 мин
- Создать пользователей: 15 мин
- Протестировать интеграцию: 30 мин
- **Итого**: ~1.5-2 часа ⚠️

---

## 🪶 Вариант 4: ZITADEL (Современный вариант)

### Что это?
API-first identity platform для microservices. Хороший баланс между функциональностью и простотой.

### Плюсы
- Меньше ресурсов чем Keycloak
- Современный API
- Хорошая документация
- Поддержка многих протоколов

### Минусы
- Требует PostgreSQL
- Более новый, меньше примеров

### Быстрый старт

```yaml
services:
  zitadel:
    image: ghcr.io/zitadel/zitadel:latest
    ports:
      - "8080:8080"
    environment:
      ZITADEL_FIRSTINSTANCE: "true"
      ZITADEL_ENVIRONMENT: development
    depends_on:
      - postgres
```

### ⏱️ Время на подключение
- Добавить в docker-compose: 10 мин
- Базовая конфигурация: 20 мин
- Протестировать: 30 мин
- **Итого**: ~1 час

---

## 📋 Рекомендуемый путь для проекта

### MVP (21.12): **fake-jwt-server**

```yaml
# ✅ Используем
services:
  fake-jwt-server:
    image: ghcr.io/stackitcloud/fake-jwt-server:latest
    ports:
      - "8008:8008"
    networks:
      - trading-network
    
  backend:
    # ... ваш код
    depends_on:
      - fake-jwt-server
```

**Преимущества**:
- 30 минут на подключение
- Не требует настройки
- Идеален для локальной разработки

### Education (28.12): **fake-jwt-server** или **ZITADEL**

```yaml
# Если time is not an issue - добавляем ZITADEL для демонстрации
services:
  zitadel:
    image: ghcr.io/zitadel/zitadel:latest
    ports:
      - "8080:8080"
    # ... конфигурация
```

### Production (путь развития): Свой Auth Service

```cpp
// Ваша реализация:
// 1. Встроенный JWT auth
// 2. Связь с вашей БД
// 3. Custom claims и permissions
```

---

## 🔗 Интеграция с backend: Пример кода

### 1. Получение токена (при login)

```cpp
// GET /api/v1/auth/login?username=user&password=pass
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

class AuthController {
public:
    std::string login(const std::string& username) {
        // Вызываем fake-jwt-server
        auto http_client = create_http_client("http://fake-jwt-server:8008");
        
        nlohmann::json payload = {
            {"sub", username},
            {"name", username}
        };
        
        auto response = http_client.post("/token", payload.dump());
        auto token_response = nlohmann::json::parse(response);
        
        return token_response["access_token"];
    }
};
```

### 2. Проверка токена (при каждом запросе)

```cpp
class AuthMiddleware {
private:
    std::string public_key_url = "http://fake-jwt-server:8008/.well-known/jwks.json";
    
public:
    bool verify_request(const Request& req) {
        auto auth_header = req.headers.get("Authorization");
        if (!auth_header || auth_header->substr(0, 7) != "Bearer ") {
            return false;
        }
        
        auto token = auth_header->substr(7);
        
        // Проверяем подпись (используем JWT lib)
        try {
            auto decoded = jwt::decode(token, get_public_key());
            req.user_id = decoded.get_claim("sub").as_string();
            return true;
        } catch (...) {
            return false;
        }
    }
};
```

### 3. Использование в контроллерах

```cpp
router.post("/api/v1/orders", [](const Request& req) {
    // Middleware уже проверил токен
    auto user_id = req.user_id;  // Получено из JWT
    
    // Создаём ордер от имени этого пользователя
    auto order = order_service->create_order(user_id, req.body);
    
    return Response::json(order);
});
```

---

## 🎯 Финальная рекомендация

### Для MVP (до 21.12)

```
🏆 ИСПОЛЬЗУЙ: fake-jwt-server
   ✅ 30 минут на подключение
   ✅ Минимум конфигурации
   ✅ Идеально для разработки
   ✅ Встроится в docker-compose в 2 строки
```

### Конфигурация docker-compose.yml

```yaml
services:
  backend:
    # ... ваш код
    environment:
      - JWT_ISSUER=http://fake-jwt-server:8008
      - JWT_PUBLIC_KEY_URL=http://fake-jwt-server:8008/.well-known/jwks.json
    depends_on:
      - fake-jwt-server
  
  fake-jwt-server:
    image: ghcr.io/stackitcloud/fake-jwt-server:latest
    ports:
      - "8008:8008"
    networks:
      - trading-network
```

---

## 📝 Адреса для подключения

### Endpoints (для вашего backend)

```bash
# Получить токен
POST http://fake-jwt-server:8008/token
Content-Type: application/json

{
  "sub": "user123",
  "name": "John Doe"
}

# Проверить публичный ключ
GET http://fake-jwt-server:8008/.well-known/jwks.json

# Health check
GET http://fake-jwt-server:8008/health
```

---

## 🚀 Чек-лист подключения (< 2 часа)

```
□ 1. Добавить fake-jwt-server в docker-compose.yml (5 мин)
□ 2. Добавить env variables в backend (3 мин)
□ 3. Создать AuthController::login() (15 мин)
□ 4. Создать AuthMiddleware::verify_token() (20 мин)
□ 5. Интегрировать middleware в router (10 мин)
□ 6. Добавить auth header к Protected endpoints (10 мин)
□ 7. Протестировать curl:
     curl -X POST http://localhost:8008/token -d '{"sub":"test"}'
     curl -H "Authorization: Bearer TOKEN" http://localhost:8080/api/v1/health (10 мин)
□ 8. docker-compose up и проверить (5 мин)

ИТОГО: 78 минут (остаётся 42 минуты запаса)
```

---

## Ссылки на документацию

- **fake-jwt-server**: https://github.com/stackitcloud/fake-jwt-server
- **Keycloak**: https://www.keycloak.org/
- **loginsrv**: https://github.com/tarentino/loginsrv
- **ZITADEL**: https://zitadel.com/
- **Authentik**: https://goauthentik.io/

