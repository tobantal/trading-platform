#include <gtest/gtest.h>

#include "adapters/primary/OrderHandler.hpp"
#include "application/OrderService.hpp"
#include "application/AuthService.hpp"
#include "application/AccountService.hpp"
#include "adapters/secondary/broker/FakeTinkoffAdapter.hpp"
#include "adapters/secondary/events/InMemoryEventBus.hpp"
#include "adapters/secondary/auth/FakeJwtAdapter.hpp"
#include "adapters/secondary/persistence/InMemoryOrderRepository.hpp"
#include "adapters/secondary/persistence/InMemoryAccountRepository.hpp"
#include "adapters/secondary/persistence/InMemoryUserRepository.hpp"

#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>

using namespace trading::adapters::primary;
using namespace trading::application;
using namespace trading::adapters::secondary;
using namespace trading::domain;

// ============================================================================
// Тестовый класс OrderHandlerTest
// ============================================================================

class OrderHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Создаем зависимости для сервисов
        broker_ = std::make_shared<FakeTinkoffAdapter>();
        eventBus_ = std::make_shared<InMemoryEventBus>();
        jwtAdapter_ = std::make_shared<FakeJwtAdapter>();
        orderRepository_ = std::make_shared<InMemoryOrderRepository>();
        accountRepository_ = std::make_shared<InMemoryAccountRepository>();
        userRepository_ = std::make_shared<InMemoryUserRepository>();
        
        // Создаем тестового пользователя и аккаунт
        User testUser;
        testUser.id = "user-123";
        testUser.username = "testuser";
        userRepository_->save(testUser);
        
        Account testAccount;
        testAccount.id = "account-456";
        testAccount.userId = "user-123";
        testAccount.name = "Test Account";
        testAccount.type = AccountType::SANDBOX;
        testAccount.isActive = true;
        accountRepository_->save(testAccount);
        
        // Создаем сервисы
        auto authService = std::make_shared<AuthService>(
            jwtAdapter_, userRepository_, accountRepository_);
        auto accountService = std::make_shared<AccountService>(accountRepository_);
        auto orderService = std::make_shared<OrderService>(
            broker_, orderRepository_, eventBus_);
        
        // Создаем handler для тестирования
        orderHandler_ = std::make_unique<OrderHandler>(
            orderService, authService, accountService);
        
        // Получаем валидный токен для тестов
        auto loginResult = authService->login("testuser");
        validToken_ = loginResult.accessToken;
        
        // Сохраняем сервисы для использования в тестах
        authService_ = authService;
        accountService_ = accountService;
        orderService_ = orderService;
    }
    
    void TearDown() override {
        orderHandler_.reset();
    }
    
    // Вспомогательный метод для парсинга JSON ответа
    nlohmann::json parseJsonResponse(SimpleResponse& res) {
        try {
            return nlohmann::json::parse(res.getBody());
        } catch (const nlohmann::json::exception& e) {
            ADD_FAILURE() << "Failed to parse JSON: " << e.what() 
                          << "\nBody: " << res.getBody();
            return nlohmann::json();
        }
    }
    
    // Вспомогательный метод для создания запроса с авторизацией
    SimpleRequest createAuthorizedRequest(
        const std::string& method, 
        const std::string& path,
        const std::string& body = "") 
    {
        std::map<std::string, std::string> headers;
        headers["Authorization"] = "Bearer " + validToken_;
        headers["Content-Type"] = "application/json";
        return SimpleRequest(method, path, body, "127.0.0.1", 8080, headers);
    }
    
    // Вспомогательный метод для создания запроса без авторизации
    SimpleRequest createUnauthorizedRequest(
        const std::string& method, 
        const std::string& path,
        const std::string& body = "") 
    {
        return SimpleRequest(method, path, body, "127.0.0.1", 8080);
    }
    
    // Вспомогательный метод для создания JSON тела ордера
    std::string createOrderBody(
        const std::string& figi = "BBG004730N88",
        const std::string& direction = "BUY",
        const std::string& type = "MARKET",
        int quantity = 10,
        double price = 0.0)
    {
        nlohmann::json body;
        body["figi"] = figi;
        body["direction"] = direction;
        body["type"] = type;
        body["quantity"] = quantity;
        if (type == "LIMIT" && price > 0) {
            body["price"] = price;
            body["currency"] = "RUB";
        }
        return body.dump();
    }
    
    std::unique_ptr<OrderHandler> orderHandler_;
    std::shared_ptr<FakeTinkoffAdapter> broker_;
    std::shared_ptr<InMemoryEventBus> eventBus_;
    std::shared_ptr<FakeJwtAdapter> jwtAdapter_;
    std::shared_ptr<InMemoryOrderRepository> orderRepository_;
    std::shared_ptr<InMemoryAccountRepository> accountRepository_;
    std::shared_ptr<InMemoryUserRepository> userRepository_;
    std::shared_ptr<AuthService> authService_;
    std::shared_ptr<AccountService> accountService_;
    std::shared_ptr<OrderService> orderService_;
    std::string validToken_;
};

// ============================================================================
// ТЕСТЫ АВТОРИЗАЦИИ
// ============================================================================

TEST_F(OrderHandlerTest, NoAuthorizationHeader_Returns401) {
    // Arrange
    SimpleRequest req = createUnauthorizedRequest("GET", "/api/v1/orders");
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 401);
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "Unauthorized");
}

TEST_F(OrderHandlerTest, InvalidToken_Returns401) {
    // Arrange
    std::map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer invalid-token-12345";
    SimpleRequest req("GET", "/api/v1/orders", "", "127.0.0.1", 8080, headers);
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 401);
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "Unauthorized");
}

TEST_F(OrderHandlerTest, MalformedAuthHeader_Returns401) {
    // Arrange - без "Bearer " префикса
    std::map<std::string, std::string> headers;
    headers["Authorization"] = validToken_;
    SimpleRequest req("GET", "/api/v1/orders", "", "127.0.0.1", 8080, headers);
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 401);
}

// ============================================================================
// ТЕСТЫ ДЛЯ POST /api/v1/orders (Создание ордера)
// ============================================================================

TEST_F(OrderHandlerTest, CreateOrder_MarketBuy_Returns201) {
    // Arrange
    std::string body = createOrderBody("BBG004730N88", "BUY", "MARKET", 10);
    SimpleRequest req = createAuthorizedRequest("POST", "/api/v1/orders", body);
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 201);
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("order_id"));
    EXPECT_TRUE(json.contains("status"));
    EXPECT_TRUE(json.contains("timestamp"));
    EXPECT_FALSE(json["order_id"].get<std::string>().empty());
}

TEST_F(OrderHandlerTest, CreateOrder_MarketSell_WithoutPosition_Returns400) {
    // Arrange - пытаемся продать без позиции
    std::string body = createOrderBody("BBG004730N88", "SELL", "MARKET", 5);
    SimpleRequest req = createAuthorizedRequest("POST", "/api/v1/orders", body);
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert - ордер отклонён, т.к. нет позиции для продажи
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("status"));
    EXPECT_EQ(json["status"], "REJECTED");
}

TEST_F(OrderHandlerTest, CreateOrder_LimitBuy_Returns201) {
    // Arrange
    nlohmann::json body;
    body["figi"] = "BBG004730N88";
    body["direction"] = "BUY";
    body["type"] = "LIMIT";
    body["quantity"] = 10;
    body["price"] = 265.50;
    body["currency"] = "RUB";
    
    SimpleRequest req = createAuthorizedRequest("POST", "/api/v1/orders", body.dump());
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 201);
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("order_id"));
}

TEST_F(OrderHandlerTest, CreateOrder_MissingFigi_Returns400) {
    // Arrange
    nlohmann::json body;
    body["direction"] = "BUY";
    body["type"] = "MARKET";
    body["quantity"] = 10;
    
    SimpleRequest req = createAuthorizedRequest("POST", "/api/v1/orders", body.dump());
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "FIGI is required");
}

TEST_F(OrderHandlerTest, CreateOrder_EmptyFigi_Returns400) {
    // Arrange
    std::string body = createOrderBody("", "BUY", "MARKET", 10);
    SimpleRequest req = createAuthorizedRequest("POST", "/api/v1/orders", body);
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJsonResponse(res);
    EXPECT_EQ(json["error"], "FIGI is required");
}

TEST_F(OrderHandlerTest, CreateOrder_ZeroQuantity_Returns400) {
    // Arrange
    std::string body = createOrderBody("BBG004730N88", "BUY", "MARKET", 0);
    SimpleRequest req = createAuthorizedRequest("POST", "/api/v1/orders", body);
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJsonResponse(res);
    EXPECT_EQ(json["error"], "Quantity must be positive");
}

TEST_F(OrderHandlerTest, CreateOrder_NegativeQuantity_Returns400) {
    // Arrange
    std::string body = createOrderBody("BBG004730N88", "BUY", "MARKET", -5);
    SimpleRequest req = createAuthorizedRequest("POST", "/api/v1/orders", body);
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJsonResponse(res);
    EXPECT_EQ(json["error"], "Quantity must be positive");
}

TEST_F(OrderHandlerTest, CreateOrder_InvalidJson_Returns400) {
    // Arrange
    SimpleRequest req = createAuthorizedRequest("POST", "/api/v1/orders", "not valid json{");
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJsonResponse(res);
    EXPECT_EQ(json["error"], "Invalid JSON");
}

TEST_F(OrderHandlerTest, CreateOrder_EmptyBody_Returns400) {
    // Arrange
    SimpleRequest req = createAuthorizedRequest("POST", "/api/v1/orders", "");
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 400);
}

// ============================================================================
// ТЕСТЫ ДЛЯ GET /api/v1/orders (Получение всех ордеров)
// ============================================================================

TEST_F(OrderHandlerTest, GetOrders_EmptyList_ReturnsEmptyArray) {
    // Arrange
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/orders");
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 0);
}

TEST_F(OrderHandlerTest, GetOrders_AfterCreatingOrder_ReturnsOrders) {
    // Arrange - сначала создаем ордер
    std::string body = createOrderBody("BBG004730N88", "BUY", "MARKET", 10);
    SimpleRequest createReq = createAuthorizedRequest("POST", "/api/v1/orders", body);
    SimpleResponse createRes;
    orderHandler_->handle(createReq, createRes);
    ASSERT_EQ(createRes.getStatus(), 201);
    
    // Act - получаем список ордеров
    SimpleRequest getReq = createAuthorizedRequest("GET", "/api/v1/orders");
    SimpleResponse getRes;
    orderHandler_->handle(getReq, getRes);
    
    // Assert
    EXPECT_EQ(getRes.getStatus(), 200);
    
    auto json = parseJsonResponse(getRes);
    EXPECT_TRUE(json.is_array());
    EXPECT_GE(json.size(), 1);
    
    // Проверяем структуру ордера
    auto firstOrder = json[0];
    EXPECT_TRUE(firstOrder.contains("id"));
    EXPECT_TRUE(firstOrder.contains("account_id"));
    EXPECT_TRUE(firstOrder.contains("figi"));
    EXPECT_TRUE(firstOrder.contains("direction"));
    EXPECT_TRUE(firstOrder.contains("type"));
    EXPECT_TRUE(firstOrder.contains("quantity"));
    EXPECT_TRUE(firstOrder.contains("status"));
    EXPECT_TRUE(firstOrder.contains("created_at"));
}

TEST_F(OrderHandlerTest, GetOrders_MultipleOrders_ReturnsAll) {
    // Arrange - создаем несколько ордеров
    for (int i = 0; i < 3; ++i) {
        std::string body = createOrderBody("BBG004730N88", "BUY", "MARKET", 10 + i);
        SimpleRequest req = createAuthorizedRequest("POST", "/api/v1/orders", body);
        SimpleResponse res;
        orderHandler_->handle(req, res);
        ASSERT_EQ(res.getStatus(), 201);
    }
    
    // Act
    SimpleRequest getReq = createAuthorizedRequest("GET", "/api/v1/orders");
    SimpleResponse getRes;
    orderHandler_->handle(getReq, getRes);
    
    // Assert
    EXPECT_EQ(getRes.getStatus(), 200);
    
    auto json = parseJsonResponse(getRes);
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 3);
}

// ============================================================================
// ТЕСТЫ ДЛЯ GET /api/v1/orders/{id} (Получение ордера по ID)
// ============================================================================

TEST_F(OrderHandlerTest, GetOrderById_ValidId_ReturnsOrder) {
    // Arrange - создаем ордер
    std::string body = createOrderBody("BBG004730N88", "BUY", "MARKET", 10);
    SimpleRequest createReq = createAuthorizedRequest("POST", "/api/v1/orders", body);
    SimpleResponse createRes;
    orderHandler_->handle(createReq, createRes);
    ASSERT_EQ(createRes.getStatus(), 201);
    
    auto createJson = parseJsonResponse(createRes);
    std::string orderId = createJson["order_id"];
    
    // Act
    SimpleRequest getReq = createAuthorizedRequest("GET", "/api/v1/orders/" + orderId);
    SimpleResponse getRes;
    orderHandler_->handle(getReq, getRes);
    
    // Assert
    EXPECT_EQ(getRes.getStatus(), 200);
    
    auto json = parseJsonResponse(getRes);
    EXPECT_EQ(json["id"], orderId);
    EXPECT_EQ(json["figi"], "BBG004730N88");
    EXPECT_EQ(json["direction"], "BUY");
    EXPECT_EQ(json["quantity"], 10);
}

TEST_F(OrderHandlerTest, GetOrderById_NonExistentId_Returns404) {
    // Arrange
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/orders/nonexistent-order-id");
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 404);
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "Order not found");
}

// ============================================================================
// ТЕСТЫ ДЛЯ DELETE /api/v1/orders/{id} (Отмена ордера)
// ============================================================================

TEST_F(OrderHandlerTest, CancelOrder_ValidPendingOrder_Returns200) {
    // Arrange - создаем ордер
    std::string body = createOrderBody("BBG004730N88", "BUY", "LIMIT", 10, 250.0);
    SimpleRequest createReq = createAuthorizedRequest("POST", "/api/v1/orders", body);
    SimpleResponse createRes;
    orderHandler_->handle(createReq, createRes);
    ASSERT_EQ(createRes.getStatus(), 201);
    
    auto createJson = parseJsonResponse(createRes);
    std::string orderId = createJson["order_id"];
    
    // Act
    SimpleRequest cancelReq = createAuthorizedRequest("DELETE", "/api/v1/orders/" + orderId);
    SimpleResponse cancelRes;
    orderHandler_->handle(cancelReq, cancelRes);
    
    // Assert
    EXPECT_EQ(cancelRes.getStatus(), 200);
    
    auto json = parseJsonResponse(cancelRes);
    EXPECT_TRUE(json.contains("message"));
    EXPECT_EQ(json["message"], "Order cancelled");
    EXPECT_EQ(json["order_id"], orderId);
}

TEST_F(OrderHandlerTest, CancelOrder_NonExistentOrder_Returns400) {
    // Arrange
    SimpleRequest req = createAuthorizedRequest("DELETE", "/api/v1/orders/nonexistent-id");
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "Cannot cancel order");
}

// ============================================================================
// ТЕСТЫ ДЛЯ НЕИЗВЕСТНЫХ ENDPOINTS
// ============================================================================

TEST_F(OrderHandlerTest, UnknownEndpoint_Returns404) {
    // Arrange - путь с несуществующим orderId
    // Примечание: /api/v1/orders/unknown/path интерпретируется как orderId="unknown/path"
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/orders/unknown-order-id");
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert - ордер не найден
    EXPECT_EQ(res.getStatus(), 404);
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "Order not found");
}

TEST_F(OrderHandlerTest, PutMethod_Returns404) {
    // Arrange
    SimpleRequest req = createAuthorizedRequest("PUT", "/api/v1/orders/order-123");
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 404);
}

// ============================================================================
// ИНТЕГРАЦИОННЫЕ ТЕСТЫ
// ============================================================================

TEST_F(OrderHandlerTest, FullOrderLifecycle_CreateGetCancel) {
    // 1. Создание ордера
    std::string body = createOrderBody("BBG004730N88", "BUY", "LIMIT", 10, 250.0);
    SimpleRequest createReq = createAuthorizedRequest("POST", "/api/v1/orders", body);
    SimpleResponse createRes;
    orderHandler_->handle(createReq, createRes);
    EXPECT_EQ(createRes.getStatus(), 201);
    
    auto createJson = parseJsonResponse(createRes);
    std::string orderId = createJson["order_id"];
    EXPECT_FALSE(orderId.empty());
    
    // 2. Получение ордера по ID
    SimpleRequest getReq = createAuthorizedRequest("GET", "/api/v1/orders/" + orderId);
    SimpleResponse getRes;
    orderHandler_->handle(getReq, getRes);
    EXPECT_EQ(getRes.getStatus(), 200);
    
    auto getJson = parseJsonResponse(getRes);
    EXPECT_EQ(getJson["id"], orderId);
    EXPECT_EQ(getJson["figi"], "BBG004730N88");
    
    // 3. Получение списка ордеров
    SimpleRequest listReq = createAuthorizedRequest("GET", "/api/v1/orders");
    SimpleResponse listRes;
    orderHandler_->handle(listReq, listRes);
    EXPECT_EQ(listRes.getStatus(), 200);
    
    auto listJson = parseJsonResponse(listRes);
    EXPECT_TRUE(listJson.is_array());
    EXPECT_GE(listJson.size(), 1);
    
    // 4. Отмена ордера
    SimpleRequest cancelReq = createAuthorizedRequest("DELETE", "/api/v1/orders/" + orderId);
    SimpleResponse cancelRes;
    orderHandler_->handle(cancelReq, cancelRes);
    EXPECT_EQ(cancelRes.getStatus(), 200);
    
    auto cancelJson = parseJsonResponse(cancelRes);
    EXPECT_EQ(cancelJson["order_id"], orderId);
}

TEST_F(OrderHandlerTest, MultipleOrderTypes_AllWork) {
    // Тестируем разные типы BUY ордеров
    // Примечание: SELL ордера требуют наличия позиции
    struct TestCase {
        std::string direction;
        std::string type;
        int quantity;
    };
    
    std::vector<TestCase> testCases = {
        {"BUY", "MARKET", 10},
        {"BUY", "MARKET", 5},
        {"BUY", "LIMIT", 15},
        {"BUY", "LIMIT", 20}
    };
    
    for (const auto& tc : testCases) {
        nlohmann::json body;
        body["figi"] = "BBG004730N88";
        body["direction"] = tc.direction;
        body["type"] = tc.type;
        body["quantity"] = tc.quantity;
        if (tc.type == "LIMIT") {
            body["price"] = 260.0;
            body["currency"] = "RUB";
        }
        
        SimpleRequest req = createAuthorizedRequest("POST", "/api/v1/orders", body.dump());
        SimpleResponse res;
        orderHandler_->handle(req, res);
        
        EXPECT_EQ(res.getStatus(), 201) 
            << "Failed for: " << tc.direction << " " << tc.type;
    }
    
    // Проверяем что все ордера созданы
    SimpleRequest listReq = createAuthorizedRequest("GET", "/api/v1/orders");
    SimpleResponse listRes;
    orderHandler_->handle(listReq, listRes);
    
    auto listJson = parseJsonResponse(listRes);
    EXPECT_EQ(listJson.size(), testCases.size());
}