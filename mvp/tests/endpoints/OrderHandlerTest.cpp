#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

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
#include "domain/Money.hpp"

#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>

using namespace trading::adapters::primary;
using namespace trading::application;
using namespace trading::adapters::secondary;
using namespace trading::domain;
using json = nlohmann::json;

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
        
        // Создаем сервисы
        authService_ = std::make_shared<AuthService>(
            jwtAdapter_, userRepository_, accountRepository_);
        accountService_ = std::make_shared<AccountService>(accountRepository_);
        orderService_ = std::make_shared<OrderService>(
            broker_, orderRepository_, eventBus_);
        
        // Регистрируем тестового пользователя
        auto regResult = authService_->registerUser("testuser", "password123");
        ASSERT_TRUE(regResult.success);
        
        // Создаем тестовый аккаунт
        trading::domain::Account testAccount(
            "account-456",
            regResult.userId,  // Используем userId из регистрации
            "Test Account",
            AccountType::SANDBOX,
            "test-token",
            true
        );
        accountRepository_->save(testAccount);
        
        // Регистрируем аккаунт в брокере
        broker_->registerAccount("account-456", "test-token");
        broker_->setCash("account-456", trading::domain::Money::fromDouble(1000000.0, "RUB"));
        
        // Создаем handler для тестирования
        orderHandler_ = std::make_unique<OrderHandler>(
            orderService_, authService_, accountService_);
        
        // Логинимся, чтобы получить session token
        auto loginResult = authService_->login("testuser", "password123");
        ASSERT_TRUE(loginResult.success);
        
        // Выбираем аккаунт, чтобы получить access token
        auto selectResult = authService_->selectAccount(loginResult.sessionToken, "account-456");
        ASSERT_TRUE(selectResult.success);
        validToken_ = selectResult.accessToken;
    }
    
    void TearDown() override {
        orderHandler_.reset();
    }
    
    // Вспомогательный метод для парсинга JSON ответа
    json parseJsonResponse(SimpleResponse& res) {
        try {
            return json::parse(res.getBody());
        } catch (const json::exception& e) {
            ADD_FAILURE() << "Failed to parse JSON: " << e.what() 
                          << "\nBody: " << res.getBody();
            return json();
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
        std::map<std::string, std::string> headers;
        headers["Content-Type"] = "application/json";
        return SimpleRequest(method, path, body, "127.0.0.1", 8080, headers);
    }
    
    // Вспомогательный метод для создания JSON тела ордера
    std::string createOrderBody(
        const std::string& figi = "BBG004730N88",
        const std::string& direction = "BUY",
        const std::string& type = "MARKET",
        int quantity = 10,
        double price = 0.0)
    {
        json body;
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
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.contains("error"));
    EXPECT_EQ(jsonResp["error"], "Unauthorized");
}

TEST_F(OrderHandlerTest, InvalidToken_Returns401) {
    // Arrange
    std::map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer invalid-token-12345";
    headers["Content-Type"] = "application/json";
    SimpleRequest req("GET", "/api/v1/orders", "", "127.0.0.1", 8080, headers);
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 401);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.contains("error"));
    EXPECT_EQ(jsonResp["error"], "Unauthorized");
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
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.contains("order_id"));
    EXPECT_TRUE(jsonResp.contains("status"));
    EXPECT_TRUE(jsonResp.contains("timestamp"));
    EXPECT_FALSE(jsonResp["order_id"].get<std::string>().empty());
}

TEST_F(OrderHandlerTest, CreateOrder_MissingFigi_Returns400) {
    // Arrange
    json body;
    body["direction"] = "BUY";
    body["type"] = "MARKET";
    body["quantity"] = 10;
    
    SimpleRequest req = createAuthorizedRequest("POST", "/api/v1/orders", body.dump());
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 400);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.contains("error"));
    EXPECT_EQ(jsonResp["error"], "FIGI is required");
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
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_EQ(jsonResp["error"], "Quantity must be positive");
}

TEST_F(OrderHandlerTest, CreateOrder_InvalidJson_Returns400) {
    // Arrange
    SimpleRequest req = createAuthorizedRequest("POST", "/api/v1/orders", "not valid json{");
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 400);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_EQ(jsonResp["error"], "Invalid JSON");
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
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.is_array());
    EXPECT_EQ(jsonResp.size(), 0);
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
    
    auto jsonResp = parseJsonResponse(getRes);
    EXPECT_TRUE(jsonResp.is_array());
    EXPECT_GE(jsonResp.size(), 1);
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
    
    auto jsonResp = parseJsonResponse(getRes);
    EXPECT_TRUE(jsonResp.contains("id"));
    EXPECT_TRUE(jsonResp.contains("figi"));
}

TEST_F(OrderHandlerTest, GetOrderById_NonExistentId_Returns404) {
    // Arrange
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/orders/nonexistent-order-id");
    SimpleResponse res;
    
    // Act
    orderHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 404);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.contains("error"));
    EXPECT_EQ(jsonResp["error"], "Order not found");
}

// ============================================================================
// ИНТЕГРАЦИОННЫЕ ТЕСТЫ
// ============================================================================

TEST_F(OrderHandlerTest, FullOrderLifecycle_CreateGet) {
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
}