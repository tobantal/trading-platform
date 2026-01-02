# Правила разработки: Как делать НЕ надо vs Как НАДО

> **Цель документа:** Систематизированные ошибки и best practices для курсового проекта  
> **Версия:** 1.0  
> **Дата:** 2025-01-02

---

## Оглавление

1. [Архитектура и Boost](#1-архитектура-и-boost)
2. [Переиспользование кода](#2-переиспользование-кода)
3. [Конфигурация и Settings](#3-конфигурация-и-settings)
4. [Kubernetes и Секреты](#4-kubernetes-и-секреты)
5. [Репозитории и БД](#5-репозитории-и-бд)
6. [CMake структура](#6-cmake-структура)
7. [RabbitMQ](#7-rabbitmq)
8. [HTTP интерфейсы](#8-http-интерфейсы)
9. [Приложение (App классы)](#9-приложение-app-классы)
10. [Тестирование](#10-тестирование)
11. [Документация](#11-документация)
12. [Graceful Shutdown](#12-graceful-shutdown)

---

## 1. Архитектура и Boost

### ❌ НЕ НАДО

```cpp
// Boost разбросан по всему коду
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/connect.hpp>

namespace beast = boost::beast;  // Упоминание boost в бизнес-логике
namespace http = beast::http;
namespace net = boost::asio;
```

### ✅ НАДО

**Правило:** Упоминание `boost` допустимо ТОЛЬКО в методе `configureInjection()` класса App.

```cpp
// В BillingApp.cpp
void BillingApp::configureInjection() {
    auto injector = di::make_injector(
        di::bind<IHttpClient>.to<BoostHttpClient>(),  // Только здесь!
        // ...
    );
}
```

**Критерий проверки:** Поиск `boost::` или `#include <boost` должен давать результаты ТОЛЬКО в:
- `configureInjection()` 
- Адаптерах в `adapters/secondary/`
- Библиотечном коде

---

## 2. Переиспользование кода

### ❌ НЕ НАДО

```cpp
// Писать свой HTTP клиент
class HttpBillingClient {
    std::string doPost(const std::string& target, const std::string& body) {
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);
        // ... 30 строк кода ...
    }
};
```

### ✅ НАДО

```cpp
// Использовать существующий IHttpClient из библиотеки
#include <IHttpClient.hpp>
#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>

class HttpBillingClient : public IBillingClient {
public:
    HttpBillingClient(std::shared_ptr<IHttpClient> client, 
                      std::shared_ptr<BillingSettings> settings)
        : client_(client), settings_(settings) {}
    
    BillingResponse charge(const std::string& userId, int64_t amount) override {
        SimpleRequest req("POST", "/api/v1/billing/charge", body.dump(),
                          settings_->getHost(), settings_->getPort());
        SimpleResponse res;
        client_->send(req, res);
        // ...
    }
};
```

**Правило:** Всегда проверять библиотеки `cpp-http-server-lib` и `common-lib` на наличие готовых решений.

---

## 3. Конфигурация и Settings

### ❌ НЕ НАДО

```cpp
// Чтение ENV прямо в классе
class HttpBillingClient {
    HttpBillingClient() {
        const char* host = std::getenv("BILLING_HOST");  // Плохо!
        const char* port = std::getenv("BILLING_PORT");
        host_ = host ? host : "billing-service";
    }
};
```

### ✅ НАДО

```cpp
// 1. Создать класс Settings
class BillingClientSettings {
public:
    BillingClientSettings() {
        const char* host = std::getenv("BILLING_HOST");
        const char* port = std::getenv("BILLING_PORT");
        
        if (!host) throw std::runtime_error("BILLING_HOST not set");
        if (!port) throw std::runtime_error("BILLING_PORT not set");
        
        host_ = host;
        port_ = std::stoi(port);
    }
    
    std::string getHost() const { return host_; }
    int getPort() const { return port_; }
    
private:
    std::string host_;
    int port_;
};

// 2. Инжектить через DI
class HttpBillingClient {
public:
    HttpBillingClient(std::shared_ptr<BillingClientSettings> settings)
        : settings_(settings) {}
};
```

**Правило:** Для каждой группы связанных настроек создавать отдельный Settings класс:
- `DbSettings` - настройки БД
- `RabbitMQSettings` - настройки RabbitMQ
- `BillingClientSettings` - настройки клиента к Billing
- и т.д.

---

## 4. Kubernetes и Секреты

### ❌ НЕ НАДО

```dockerfile
# В Dockerfile
ENV DB_PASSWORD=secret123
ENV RABBITMQ_PASSWORD=rabbit_pass
```

### ✅ НАДО

```yaml
# k8s/secret.yaml
apiVersion: v1
kind: Secret
metadata:
  name: db-secret
type: Opaque
stringData:
  DB_USER: "app_user"
  DB_PASSWORD: "secure_password"

---
# k8s/deployment.yaml
env:
  - name: DB_PASSWORD
    valueFrom:
      secretKeyRef:
        name: db-secret
        key: DB_PASSWORD
```

**Правило:** Все секреты (пароли, токены, ключи) хранить ТОЛЬКО в `k8s/secret.yaml`.

---

## 5. Репозитории и БД

### ❌ НЕ НАДО

```cpp
// InMemory репозитории для production
class InMemoryOrderRepository : public IOrderRepository {
    std::map<std::string, Order> orders_;  // Теряем данные при рестарте!
};
```

### ✅ НАДО

```cpp
// PostgreSQL репозитории
class PostgresOrderRepository : public IOrderRepository {
public:
    PostgresOrderRepository(std::shared_ptr<DbSettings> settings) 
        : settings_(settings) {
        initSchema();  // CREATE TABLE IF NOT EXISTS
    }
    
    void save(const Order& order) override {
        pqxx::connection c(settings_->getConnectionString());
        pqxx::work t(c);
        t.exec_params("INSERT INTO orders ...", order.id, ...);
        t.commit();
    }
};
```

**Правило:** InMemory репозитории допускаются ТОЛЬКО для unit-тестов, не для deployment.

---

## 6. CMake структура

### ❌ НЕ НАДО

```cmake
# В корневом CMakeLists.txt - перечисление подпроектов подпроекта
add_subdirectory(hw-07-events/billing-service)
add_subdirectory(hw-07-events/notification-service)
add_subdirectory(hw-07-events/order-service)
```

### ✅ НАДО

```cmake
# Корневой CMakeLists.txt
add_subdirectory(hw-07-events)

# hw-07-events/CMakeLists.txt
add_subdirectory(billing-service)
add_subdirectory(notification-service)
add_subdirectory(order-service)
```

**Правило:** Каждый подпроект отвечает за свои подпроекты.

---

## 7. RabbitMQ

### ❌ НЕ НАДО

```cpp
// 1. Кастомный TcpHandler
class TcpHandler : public AMQP::TcpHandler {
    void monitor(AMQP::TcpConnection*, int, int) override {}  // Пустой!
};

// 2. Типизированные события (плохо для микросервисов)
void publish(const OrderCreatedEvent& event);
```

### ✅ НАДО

```cpp
// 1. Использовать LibBoostAsioHandler из AMQP-CPP
#include <amqpcpp/libboostasio.h>

ioContext_ = std::make_unique<boost::asio::io_context>();
handler_ = std::make_unique<AMQP::LibBoostAsioHandler>(*ioContext_);
connection_ = std::make_unique<AMQP::TcpConnection>(handler_.get(), address);

// 2. Строковые сообщения (JSON)
void publish(const std::string& routingKey, const std::string& jsonMessage);

// Пример использования
nlohmann::json event = {
    {"order_id", orderId},
    {"user_id", userId},
    {"amount", amount}
};
publisher_->publish("order.created", event.dump());
```

**Правило:** Сообщения в RabbitMQ - всегда JSON строки, не типизированные объекты.

### initContainers для ожидания зависимостей

```yaml
# k8s/service.yaml
spec:
  initContainers:
  - name: wait-for-rabbitmq
    image: busybox
    command: ['sh', '-c', 'until nc -z rabbitmq 5672; do echo waiting; sleep 2; done']
  - name: wait-for-postgres
    image: busybox
    command: ['sh', '-c', 'until nc -z postgres 5432; do echo waiting; sleep 2; done']
  containers:
  - name: service
    # ...
```

**Правило:** Всегда добавлять initContainers для ожидания зависимостей (RabbitMQ, PostgreSQL).

---

## 8. HTTP интерфейсы

### ❌ НЕ НАДО

```cpp
// Использование несуществующих методов
auto key = req.getHeader("X-Idempotency-Key");  // НЕТ такого метода!
int status = res.getStatus();  // НЕТ такого метода!
```

### ✅ НАДО

```cpp
// IRequest: есть только getHeaders() возвращающий map
auto headers = req.getHeaders();
auto it = headers.find("X-Idempotency-Key");
std::string key = (it != headers.end()) ? it->second : "";

// IResponse: только setters, нет getters
// Для перехвата статуса - использовать wrapper
class ResponseCapture : public IResponse {
public:
    explicit ResponseCapture(IResponse& inner) : inner_(inner) {}
    
    void setStatus(int code) override {
        status_ = code;
        inner_.setStatus(code);
    }
    
    void setBody(const std::string& body) override {
        body_ = body;
        inner_.setBody(body);
    }
    
    int getStatus() const { return status_; }
    std::string getBody() const { return body_; }
private:
    IResponse& inner_;
    int status_ = 0;
    std::string body_;
};
```

**Правило:** Всегда проверять сигнатуры интерфейсов в `cpp-http-server-lib.txt` перед использованием.

---

## 9. Приложение (App классы)

### ❌ НЕ НАДО

```cpp
// Хранение сервисов как полей класса
class OrderApp : public BoostBeastApplication {
private:
    std::shared_ptr<IOrderRepository> repository_;  // НЕ хранить!
    std::shared_ptr<OrderService> service_;
    std::shared_ptr<OrderHandler> handler_;
};
```

### ✅ НАДО

```cpp
class OrderApp : public BoostBeastApplication {
public:
    void configureInjection() override {
        auto injector = di::make_injector(/* ... */);
        
        // Создаём и сразу регистрируем, не храним
        auto handler = injector.create<std::shared_ptr<OrderHandler>>();
        handlers_[getHandlerKey("POST", "/api/v1/orders")] = handler;
    }
    // Нет private полей для сервисов!
};
```

**Правило:** App класс только конфигурирует DI и регистрирует хэндлеры, не хранит зависимости.

---

## 10. Тестирование

### ❌ НЕ НАДО

```javascript
// Postman: неправильная проверка отсутствия заголовка
pm.expect(pm.response.headers.get('X-Header')).to.be.null;  // undefined !== null!
```

### ✅ НАДО

```javascript
// Правильная проверка
pm.expect(pm.response.headers.has('X-Header')).to.be.false;

// Или
pm.expect(pm.response.headers.get('X-Header')).to.be.undefined;
```

### Именование тестов

```javascript
// Чёткие названия тестов
pm.test('First request: Order accepted', function() { ... });
pm.test('Retry returns same order_id', function() { ... });
pm.test('Balance charged only once (900)', function() { ... });
```

---

## 11. Документация

### ❌ НЕ НАДО

```cpp
class IBillingService {
public:
    virtual ~IBillingService() = default;
    virtual domain::BillingAccount createAccount(const std::string& userId) = 0;  // Что делает? Непонятно
};
```

### ✅ НАДО

```cpp
/**
 * @file IBillingService.hpp
 * @brief Интерфейс сервиса биллинга
 * @author Anton Tobolkin
 */

/**
 * @class IBillingService
 * @brief Управление аккаунтами и транзакциями пользователей
 */
class IBillingService {
public:
    virtual ~IBillingService() = default;
    
    /**
     * @brief Создать новый аккаунт пользователя
     * @param userId Идентификатор пользователя
     * @return Созданный аккаунт с нулевым балансом
     * @throws std::runtime_error если аккаунт уже существует
     */
    virtual domain::BillingAccount createAccount(const std::string& userId) = 0;
};
```

---

## 12. Graceful Shutdown

### ❌ НЕ НАДО

```cpp
int main(int argc, char* argv[]) {
    try {
        BillingApp app;
        app.run(argc, argv);  // Просто запуск
    } catch (...) { }
    return 0;
}
```

### ✅ НАДО

```cpp
#include <csignal>

billing::BillingApp* g_app = nullptr;

void signalHandler(int signal) {
    std::cout << "\n[main] Received signal " << signal << std::endl;
    if (g_app) {
        g_app->stop();
    }
}

int main(int argc, char* argv[]) {
    try {
        billing::BillingApp app;
        g_app = &app;

        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        std::cout << "========================================" << std::endl;
        std::cout << "  Billing Service Starting" << std::endl;
        std::cout << "========================================" << std::endl;

        app.run(argc, argv);

        std::cout << "  Billing Service Stopped" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[main] Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

**Правило:** Signal handlers для graceful shutdown в Kubernetes (SIGTERM при остановке pod).

---

## Чеклист перед commit

- [ ] Нет `boost::` вне `configureInjection()` и адаптеров
- [ ] Переиспользован код из библиотек где возможно
- [ ] Все ENV читаются через Settings классы
- [ ] Секреты в `k8s/secret.yaml`
- [ ] PostgreSQL репозитории (не InMemory)
- [ ] RabbitMQ через `LibBoostAsioHandler`
- [ ] Сообщения как JSON строки
- [ ] initContainers для зависимостей
- [ ] Signal handlers в main.cpp
- [ ] Doxygen документация в интерфейсах
- [ ] Postman тесты корректны

---

## 13. Docker и Kubernetes деплой

### ❌ НЕ НАДО

```yaml
# В k8s deployment - локальный образ
containers:
  - name: auth-service
    image: auth-service:latest           # Локальный образ!
    imagePullPolicy: IfNotPresent        # Не подтянет из registry!
```

В README - без инструкций для DockerHub
```bash
kubectl apply -f k8s/auth-service.yaml   # А откуда образ?
```

### ✅ НАДО

```yaml
# В k8s deployment - образ из DockerHub
containers:
  - name: auth-service
    image: tobantal/auth-service:latest  # DockerHub образ!
    imagePullPolicy: Always              # Всегда подтягивать!
```

README должен содержать полные инструкции:

### 1. Сборка и публикация Docker образа

```bash
# Из корня проекта
docker build -t tobantal/auth-service:latest -f auth-service/Dockerfile .

# Логин в DockerHub
docker login

# Публикация образа
docker push tobantal/auth-service:latest
```

### 2. Развёртывание в Kubernetes

```bash
kubectl apply -f k8s/namespace.yaml
kubectl apply -f k8s/secret.yaml
kubectl apply -f k8s/auth-postgres.yaml
kubectl apply -f k8s/auth-service.yaml
```

**Правило:** Образы всегда публикуются в DockerHub (`tobantal/...`) перед деплоем в Kubernetes.

---

## Дополнение к чеклисту

- [ ] В k8s/*.yaml используется `image: tobantal/<service>:latest`
- [ ] В k8s/*.yaml установлен `imagePullPolicy: Always`
- [ ] В README есть секция "Docker build & push" ПЕРЕД kubectl apply

----

## Quick Reference: Интерфейсы библиотеки

### IRequest
```cpp
std::string getPath() const;
std::string getMethod() const;
std::string getBody() const;
std::map<std::string, std::string> getParams() const;
std::map<std::string, std::string> getHeaders() const;  // НЕ getHeader()!
std::string getIp() const;
int getPort() const;
```

### IResponse
```cpp
void setStatus(int code);
void setBody(const std::string& body);
void setHeader(const std::string& name, const std::string& value);
// НЕТ getters!
```

### IHttpHandler
```cpp
void handle(IRequest& req, IResponse& res);
```
