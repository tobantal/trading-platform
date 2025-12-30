// tests/integration/TradingFlowIntegrationTest.cpp
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <random>

// Handlers
#include "adapters/primary/auth/LoginHandler.hpp"
#include "adapters/primary/auth/SelectAccountHandler.hpp"
#include "adapters/primary/OrderHandler.hpp"
#include "adapters/primary/PortfolioHandler.hpp"

// Services
#include "application/AuthService.hpp"
#include "application/OrderService.hpp"
#include "application/PortfolioService.hpp"

// Adapters
#include "adapters/secondary/broker/SimpleBrokerGatewayAdapter.hpp"
#include "adapters/secondary/events/InMemoryEventBus.hpp"
#include "adapters/secondary/auth/FakeJwtAdapter.hpp"
#include "adapters/secondary/persistence/InMemoryOrderRepository.hpp"
#include "adapters/secondary/persistence/InMemoryAccountRepository.hpp"
#include "adapters/secondary/persistence/InMemoryUserRepository.hpp"

// HTTP helpers
#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>

using namespace trading::adapters::primary;
using namespace trading::application;
using namespace trading::adapters::secondary;
using namespace trading::domain;
using json = nlohmann::json;

/**
 * @brief Интеграционный тест полного торгового flow
 */
class TradingFlowIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Генерируем уникальный суффикс для имени пользователя
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000, 999999);
        uniqueSuffix_ = std::to_string(dis(gen));
        
        // ================================================================
        // INFRASTRUCTURE LAYER
        // ================================================================
        broker_ = std::make_shared<SimpleBrokerGatewayAdapter>();
        eventBus_ = std::make_shared<InMemoryEventBus>();
        jwtAdapter_ = std::make_shared<FakeJwtAdapter>();
        orderRepository_ = std::make_shared<InMemoryOrderRepository>();
        accountRepository_ = std::make_shared<InMemoryAccountRepository>();
        userRepository_ = std::make_shared<InMemoryUserRepository>();
        
        // ================================================================
        // APPLICATION LAYER (SERVICES)
        // ================================================================
        authService_ = std::make_shared<AuthService>(
            jwtAdapter_, userRepository_, accountRepository_);
        orderService_ = std::make_shared<OrderService>(
            broker_, orderRepository_, eventBus_);
        portfolioService_ = std::make_shared<PortfolioService>(
            broker_, accountRepository_);
        
        // ================================================================
        // PRIMARY ADAPTERS (HANDLERS)
        // ================================================================
        loginHandler_ = std::make_unique<LoginHandler>(authService_);
        selectAccountHandler_ = std::make_unique<SelectAccountHandler>(authService_);
        orderHandler_ = std::make_unique<OrderHandler>(orderService_, authService_);
        portfolioHandler_ = std::make_unique<PortfolioHandler>(portfolioService_, authService_);
        
        // ================================================================
        // TEST DATA SETUP
        // ================================================================
        setupTestUser();
    }
    
    void setupTestUser() {
        // Используем уникальное имя пользователя
        testUsername_ = "test_trader_" + uniqueSuffix_;
        testPassword_ = "test_password_123";
        
        // Регистрируем тестового пользователя
        auto regResult = authService_->registerUser(testUsername_, testPassword_);
        ASSERT_TRUE(regResult.success) << "Failed to register user: " << regResult.error;
        testUserId_ = regResult.userId;
        
        // Создаём уникальный тестовый аккаунт
        testAccountId_ = "acc-test-" + uniqueSuffix_;
        Account testAccount(
            testAccountId_,
            testUserId_,
            "Test Sandbox Account",
            AccountType::SANDBOX,
            "test-broker-token-" + uniqueSuffix_,
            true
        );
        accountRepository_->save(testAccount);
        
        // Настраиваем FakeBroker
        broker_->registerAccount(testAccountId_, "test-broker-token-" + uniqueSuffix_);
        broker_->setCash(testAccountId_, Money::fromDouble(100000.0, "RUB"));
    }
    
    // ================================================================
    // HELPER METHODS
    // ================================================================
    
    json parseJson(SimpleResponse& res) {
        try {
            return json::parse(res.getBody());
        } catch (const json::exception& e) {
            ADD_FAILURE() << "JSON parse error: " << e.what() 
                          << "\nBody: " << res.getBody();
            return json();
        }
    }
    
    SimpleRequest makeRequest(
        const std::string& method,
        const std::string& path,
        const std::string& body = "",
        const std::string& token = "")
    {
        std::map<std::string, std::string> headers;
        headers["Content-Type"] = "application/json";
        if (!token.empty()) {
            headers["Authorization"] = "Bearer " + token;
        }
        return SimpleRequest(method, path, body, "127.0.0.1", 8080, headers);
    }
    
    // ================================================================
    // TEST INFRASTRUCTURE
    // ================================================================
    
    std::shared_ptr<SimpleBrokerGatewayAdapter> broker_;
    std::shared_ptr<InMemoryEventBus> eventBus_;
    std::shared_ptr<FakeJwtAdapter> jwtAdapter_;
    std::shared_ptr<InMemoryOrderRepository> orderRepository_;
    std::shared_ptr<InMemoryAccountRepository> accountRepository_;
    std::shared_ptr<InMemoryUserRepository> userRepository_;
    
    std::shared_ptr<AuthService> authService_;
    std::shared_ptr<OrderService> orderService_;
    std::shared_ptr<PortfolioService> portfolioService_;
    
    std::unique_ptr<LoginHandler> loginHandler_;
    std::unique_ptr<SelectAccountHandler> selectAccountHandler_;
    std::unique_ptr<OrderHandler> orderHandler_;
    std::unique_ptr<PortfolioHandler> portfolioHandler_;
    
    std::string uniqueSuffix_;
    std::string testUsername_;
    std::string testPassword_;
    std::string testUserId_;
    std::string testAccountId_;
};

// ============================================================================
// INTEGRATION TEST: Full Trading Flow
// ============================================================================

TEST_F(TradingFlowIntegrationTest, FullFlow_LoginSelectAccountPlaceOrder) {
    // ================================================================
    // STEP 1: LOGIN
    // ================================================================
    json loginBody;
    loginBody["username"] = testUsername_;
    loginBody["password"] = testPassword_;
    
    SimpleRequest loginReq = makeRequest("POST", "/api/v1/auth/login", loginBody.dump());
    SimpleResponse loginRes;
    loginHandler_->handle(loginReq, loginRes);
    
    ASSERT_EQ(loginRes.getStatus(), 200) << "Login failed: " << loginRes.getBody();
    
    auto loginJson = parseJson(loginRes);
    ASSERT_TRUE(loginJson.contains("session_token")) << "No session_token in response";
    
    std::string sessionToken = loginJson["session_token"];
    ASSERT_FALSE(sessionToken.empty());
    
    // ================================================================
    // STEP 2: SELECT ACCOUNT
    // ================================================================
    json selectBody;
    selectBody["account_id"] = testAccountId_;
    
    SimpleRequest selectReq = makeRequest(
        "POST", "/api/v1/auth/select-account", 
        selectBody.dump(), sessionToken);
    SimpleResponse selectRes;
    selectAccountHandler_->handle(selectReq, selectRes);
    
    ASSERT_EQ(selectRes.getStatus(), 200) << "Select account failed: " << selectRes.getBody();
    
    auto selectJson = parseJson(selectRes);
    ASSERT_TRUE(selectJson.contains("access_token")) << "No access_token in response";
    
    std::string accessToken = selectJson["access_token"];
    ASSERT_FALSE(accessToken.empty());
    
    // ================================================================
    // STEP 3: CHECK PORTFOLIO
    // ================================================================
    SimpleRequest portfolioReq = makeRequest(
        "GET", "/api/v1/portfolio", "", accessToken);
    SimpleResponse portfolioRes;
    portfolioHandler_->handle(portfolioReq, portfolioRes);
    
    ASSERT_EQ(portfolioRes.getStatus(), 200) << "Portfolio request failed: " << portfolioRes.getBody();
    
    auto portfolioJson = parseJson(portfolioRes);
    ASSERT_TRUE(portfolioJson.contains("account_id"));
    ASSERT_TRUE(portfolioJson.contains("cash"));
    
    // ================================================================
    // STEP 4: PLACE ORDER
    // ================================================================
    json orderBody;
    orderBody["figi"] = "BBG004730N88";  // SBER
    orderBody["direction"] = "BUY";
    orderBody["type"] = "MARKET";
    orderBody["quantity"] = 10;
    
    SimpleRequest orderReq = makeRequest(
        "POST", "/api/v1/orders", orderBody.dump(), accessToken);
    SimpleResponse orderRes;
    orderHandler_->handle(orderReq, orderRes);
    
    ASSERT_EQ(orderRes.getStatus(), 201) << "Order creation failed: " << orderRes.getBody();
    
    auto orderJson = parseJson(orderRes);
    ASSERT_TRUE(orderJson.contains("order_id")) << "No order_id in response";
    
    std::string orderId = orderJson["order_id"];
    ASSERT_FALSE(orderId.empty());
    
    // ================================================================
    // STEP 5: VERIFY ORDER IN LIST
    // ================================================================
    SimpleRequest listReq = makeRequest("GET", "/api/v1/orders", "", accessToken);
    SimpleResponse listRes;
    orderHandler_->handle(listReq, listRes);
    
    ASSERT_EQ(listRes.getStatus(), 200) << "Order list failed: " << listRes.getBody();
    
    auto listJson = parseJson(listRes);
    ASSERT_TRUE(listJson.contains("orders"));
    ASSERT_TRUE(listJson["orders"].is_array());
    ASSERT_GE(listJson["orders"].size(), 1) << "Order list should have at least 1 order";
}

// ============================================================================
// ERROR CASES
// ============================================================================

TEST_F(TradingFlowIntegrationTest, OrderWithoutAccessToken_Returns401) {
    json orderBody;
    orderBody["figi"] = "BBG004730N88";
    orderBody["direction"] = "BUY";
    orderBody["type"] = "MARKET";
    orderBody["quantity"] = 10;
    
    SimpleRequest req = makeRequest("POST", "/api/v1/orders", orderBody.dump());
    SimpleResponse res;
    orderHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 401);
}

TEST_F(TradingFlowIntegrationTest, OrderWithSessionToken_Returns401) {
    // Логинимся
    json loginBody;
    loginBody["username"] = testUsername_;
    loginBody["password"] = testPassword_;
    
    SimpleRequest loginReq = makeRequest("POST", "/api/v1/auth/login", loginBody.dump());
    SimpleResponse loginRes;
    loginHandler_->handle(loginReq, loginRes);
    ASSERT_EQ(loginRes.getStatus(), 200);
    
    auto loginJson = parseJson(loginRes);
    std::string sessionToken = loginJson["session_token"];
    
    // Пытаемся создать ордер с SESSION token (не access!)
    json orderBody;
    orderBody["figi"] = "BBG004730N88";
    orderBody["direction"] = "BUY";
    orderBody["type"] = "MARKET";
    orderBody["quantity"] = 10;
    
    SimpleRequest orderReq = makeRequest(
        "POST", "/api/v1/orders", orderBody.dump(), sessionToken);
    SimpleResponse orderRes;
    orderHandler_->handle(orderReq, orderRes);
    
    // Session token не содержит accountId, поэтому должен быть 401
    EXPECT_EQ(orderRes.getStatus(), 401) 
        << "Should reject session token for trading operations";
}

TEST_F(TradingFlowIntegrationTest, SelectAccountWithInvalidId_Returns404) {
    // Логинимся
    json loginBody;
    loginBody["username"] = testUsername_;
    loginBody["password"] = testPassword_;
    
    SimpleRequest loginReq = makeRequest("POST", "/api/v1/auth/login", loginBody.dump());
    SimpleResponse loginRes;
    loginHandler_->handle(loginReq, loginRes);
    ASSERT_EQ(loginRes.getStatus(), 200);
    
    auto loginJson = parseJson(loginRes);
    std::string sessionToken = loginJson["session_token"];
    
    // Пытаемся выбрать несуществующий аккаунт
    json selectBody;
    selectBody["account_id"] = "nonexistent-account-xyz";
    
    SimpleRequest selectReq = makeRequest(
        "POST", "/api/v1/auth/select-account",
        selectBody.dump(), sessionToken);
    SimpleResponse selectRes;
    selectAccountHandler_->handle(selectReq, selectRes);
    
    EXPECT_EQ(selectRes.getStatus(), 404);
}
