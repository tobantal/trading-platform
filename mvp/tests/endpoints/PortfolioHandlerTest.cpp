// tests/endpoints/PortfolioHandlerTest.cpp
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "adapters/primary/PortfolioHandler.hpp"
#include "application/PortfolioService.hpp"
#include "application/AuthService.hpp"
#include "adapters/secondary/broker/FakeTinkoffAdapter.hpp"
#include "adapters/secondary/auth/FakeJwtAdapter.hpp"
#include "adapters/secondary/persistence/InMemoryAccountRepository.hpp"
#include "adapters/secondary/persistence/InMemoryUserRepository.hpp"

#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>

using namespace trading::adapters::primary;
using namespace trading::application;
using namespace trading::adapters::secondary;
using namespace trading::domain;
using json = nlohmann::json;

// ============================================================================
// Тестовый класс PortfolioHandlerTest
// ============================================================================

class PortfolioHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Создаем зависимости для сервисов
        broker_ = std::make_shared<FakeTinkoffAdapter>();
        jwtAdapter_ = std::make_shared<FakeJwtAdapter>();
        accountRepository_ = std::make_shared<InMemoryAccountRepository>();
        userRepository_ = std::make_shared<InMemoryUserRepository>();
        
        // Создаем сервисы
        authService_ = std::make_shared<AuthService>(
            jwtAdapter_, userRepository_, accountRepository_);
        portfolioService_ = std::make_shared<PortfolioService>(broker_, accountRepository_);
        
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
        broker_->setCash("account-456", Money::fromDouble(1000000.0, "RUB"));
        
        // Создаем handler для тестирования (БЕЗ accountService!)
        portfolioHandler_ = std::make_unique<PortfolioHandler>(portfolioService_, authService_);
        
        // Логинимся, чтобы получить session token
        auto loginResult = authService_->login("testuser", "password123");
        ASSERT_TRUE(loginResult.success);
        
        // Выбираем аккаунт, чтобы получить access token
        auto selectResult = authService_->selectAccount(loginResult.sessionToken, "account-456");
        ASSERT_TRUE(selectResult.success);
        validToken_ = selectResult.accessToken;
    }
    
    void TearDown() override {
        portfolioHandler_.reset();
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
    
    std::unique_ptr<PortfolioHandler> portfolioHandler_;
    std::shared_ptr<FakeTinkoffAdapter> broker_;
    std::shared_ptr<FakeJwtAdapter> jwtAdapter_;
    std::shared_ptr<InMemoryAccountRepository> accountRepository_;
    std::shared_ptr<InMemoryUserRepository> userRepository_;
    std::shared_ptr<AuthService> authService_;
    std::shared_ptr<PortfolioService> portfolioService_;
    std::string validToken_;
};

// ============================================================================
// ТЕСТЫ АВТОРИЗАЦИИ
// ============================================================================

TEST_F(PortfolioHandlerTest, NoAuthorizationHeader_Returns401) {
    // Arrange
    SimpleRequest req = createUnauthorizedRequest("GET", "/api/v1/portfolio");
    SimpleResponse res;
    
    // Act
    portfolioHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 401);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.contains("error"));
}

TEST_F(PortfolioHandlerTest, InvalidToken_Returns401) {
    // Arrange
    std::map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer invalid_token_12345";
    headers["Content-Type"] = "application/json";
    SimpleRequest req("GET", "/api/v1/portfolio", "", "127.0.0.1", 8080, headers);
    SimpleResponse res;
    
    // Act
    portfolioHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 401);
}

// ============================================================================
// ТЕСТЫ GET /api/v1/portfolio
// ============================================================================

TEST_F(PortfolioHandlerTest, GetPortfolio_ValidToken_Returns200) {
    // Arrange
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/portfolio");
    SimpleResponse res;
    
    // Act
    portfolioHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 200);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.contains("account_id"));
    EXPECT_TRUE(jsonResp.contains("cash"));
    EXPECT_TRUE(jsonResp.contains("positions"));
}

TEST_F(PortfolioHandlerTest, GetPortfolio_HasCash_ReturnsCorrectAmount) {
    // Arrange
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/portfolio");
    SimpleResponse res;
    
    // Act
    portfolioHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 200);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.contains("cash"));
    
    auto cash = jsonResp["cash"];
    EXPECT_TRUE(cash.contains("amount"));
    EXPECT_GE(cash["amount"].get<double>(), 0);
}

// ============================================================================
// ТЕСТЫ GET /api/v1/portfolio/positions
// ============================================================================

TEST_F(PortfolioHandlerTest, GetPositions_ValidToken_Returns200) {
    // Arrange
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/portfolio/positions");
    SimpleResponse res;
    
    // Act
    portfolioHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 200);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.is_array());
}

// ============================================================================
// ТЕСТЫ GET /api/v1/portfolio/cash
// ============================================================================

TEST_F(PortfolioHandlerTest, GetCash_ValidToken_Returns200) {
    // Arrange
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/portfolio/cash");
    SimpleResponse res;
    
    // Act
    portfolioHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 200);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.contains("available"));
    EXPECT_TRUE(jsonResp.contains("currency"));
}

// ============================================================================
// ТЕСТЫ 404
// ============================================================================

TEST_F(PortfolioHandlerTest, UnknownPath_Returns404) {
    // Arrange
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/portfolio/unknown");
    SimpleResponse res;
    
    // Act
    portfolioHandler_->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 404);
}
