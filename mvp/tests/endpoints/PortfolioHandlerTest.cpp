#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "adapters/primary/PortfolioHandler.hpp"
#include "application/PortfolioService.hpp"
#include "application/AuthService.hpp"
#include "application/AccountService.hpp"
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
        accountService_ = std::make_shared<AccountService>(accountRepository_);
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
        
        // Создаем handler для тестирования
        portfolioHandler_ = std::make_unique<PortfolioHandler>(
            portfolioService_, authService_, accountService_);
        
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
    std::shared_ptr<AccountService> accountService_;
    std::shared_ptr<PortfolioService> portfolioService_;
    std::string validToken_;
};

// ============================================================================
// ТЕСТЫ АВТОРИЗАЦИИ
// ============================================================================

TEST_F(PortfolioHandlerTest, NoAuthorizationHeader_Returns401) {
    SimpleRequest req = createUnauthorizedRequest("GET", "/api/v1/portfolio");
    SimpleResponse res;
    
    portfolioHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 401);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.contains("error"));
    EXPECT_EQ(jsonResp["error"], "Unauthorized");
}

TEST_F(PortfolioHandlerTest, InvalidToken_Returns401) {
    std::map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer invalid-token-12345";
    headers["Content-Type"] = "application/json";
    SimpleRequest req("GET", "/api/v1/portfolio", "", "127.0.0.1", 8080, headers);
    SimpleResponse res;
    
    portfolioHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 401);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.contains("error"));
    EXPECT_EQ(jsonResp["error"], "Unauthorized");
}

TEST_F(PortfolioHandlerTest, MalformedAuthHeader_Returns401) {
    std::map<std::string, std::string> headers;
    headers["Authorization"] = validToken_; // без "Bearer "
    headers["Content-Type"] = "application/json";
    SimpleRequest req("GET", "/api/v1/portfolio", "", "127.0.0.1", 8080, headers);
    SimpleResponse res;
    
    portfolioHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 401);
}

// ============================================================================
// ТЕСТЫ ДЛЯ GET /api/v1/portfolio (Получение полного портфеля)
// ============================================================================

TEST_F(PortfolioHandlerTest, GetPortfolio_Returns200) {
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/portfolio");
    SimpleResponse res;
    
    portfolioHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.contains("account_id"));
    EXPECT_EQ(jsonResp["account_id"], "account-456");
    EXPECT_TRUE(jsonResp.contains("cash"));
    EXPECT_TRUE(jsonResp.contains("total_value"));
    EXPECT_TRUE(jsonResp.contains("total_pnl"));
    EXPECT_TRUE(jsonResp.contains("pnl_percent"));
    EXPECT_TRUE(jsonResp.contains("positions"));
    
    auto cash = jsonResp["cash"];
    EXPECT_TRUE(cash.contains("amount"));
    EXPECT_TRUE(cash.contains("currency"));
    EXPECT_EQ(cash["currency"], "RUB");
}

TEST_F(PortfolioHandlerTest, GetPortfolio_WithPositions_ReturnsPositionsArray) {
    std::vector<trading::domain::Position> positions = {
        trading::domain::Position("BBG004730N88", "SBER", 100, 
                                 Money::fromDouble(265.0, "RUB"),
                                 Money::fromDouble(270.0, "RUB"))
    };
    broker_->setPositions("account-456", positions);
    
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/portfolio");
    SimpleResponse res;
    
    portfolioHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto jsonResp = parseJsonResponse(res);
    auto positionsArray = jsonResp["positions"];
    EXPECT_TRUE(positionsArray.is_array());
    EXPECT_GT(positionsArray.size(), 0);
    
    auto position = positionsArray[0];
    EXPECT_TRUE(position.contains("figi"));
    EXPECT_TRUE(position.contains("ticker"));
    EXPECT_TRUE(position.contains("quantity"));
    EXPECT_TRUE(position.contains("average_price"));
    EXPECT_TRUE(position.contains("current_price"));
    EXPECT_TRUE(position.contains("currency"));
    EXPECT_TRUE(position.contains("pnl"));
    EXPECT_TRUE(position.contains("pnl_percent"));
}

// ============================================================================
// ТЕСТЫ ДЛЯ GET /api/v1/portfolio/positions (Получение позиций)
// ============================================================================

TEST_F(PortfolioHandlerTest, GetPositions_Returns200) {
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/portfolio/positions");
    SimpleResponse res;
    
    portfolioHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.is_array());
    
    for (const auto& position : jsonResp) {
        EXPECT_TRUE(position.contains("figi"));
        EXPECT_TRUE(position.contains("ticker"));
        EXPECT_TRUE(position.contains("quantity"));
    }
}

TEST_F(PortfolioHandlerTest, GetPositions_WithMultiplePositions_ReturnsAll) {
    std::vector<trading::domain::Position> positions = {
        trading::domain::Position("BBG004730N88", "SBER", 100, 
                                 Money::fromDouble(265.0, "RUB"),
                                 Money::fromDouble(270.0, "RUB")),
        trading::domain::Position("BBG004731032", "LKOH", 10, 
                                 Money::fromDouble(7200.0, "RUB"),
                                 Money::fromDouble(7300.0, "RUB"))
    };
    broker_->setPositions("account-456", positions);
    
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/portfolio/positions");
    SimpleResponse res;
    
    portfolioHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.is_array());
    EXPECT_EQ(jsonResp.size(), 2);
}

// ============================================================================
// ТЕСТЫ ДЛЯ GET /api/v1/portfolio/cash (Получение доступных средств)
// ============================================================================

TEST_F(PortfolioHandlerTest, GetCash_Returns200) {
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/portfolio/cash");
    SimpleResponse res;
    
    portfolioHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.contains("available"));
    EXPECT_TRUE(jsonResp.contains("currency"));
    EXPECT_EQ(jsonResp["currency"], "RUB");
}

TEST_F(PortfolioHandlerTest, GetCash_WithZeroCash_ReturnsZero) {
    broker_->setCash("account-456", Money::fromDouble(0.0, "RUB"));
    
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/portfolio/cash");
    SimpleResponse res;
    
    portfolioHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_EQ(jsonResp["available"], 0.0);
    EXPECT_EQ(jsonResp["currency"], "RUB");
}

// ============================================================================
// ТЕСТЫ ДЛЯ НЕИЗВЕСТНЫХ ENDPOINTS
// ============================================================================

TEST_F(PortfolioHandlerTest, UnknownEndpoint_Returns404) {
    SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/portfolio/unknown");
    SimpleResponse res;
    
    portfolioHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
    
    auto jsonResp = parseJsonResponse(res);
    EXPECT_TRUE(jsonResp.contains("error"));
    EXPECT_EQ(jsonResp["error"], "Not found");
}

// ============================================================================
// ИНТЕГРАЦИОННЫЕ ТЕСТЫ
// ============================================================================

TEST_F(PortfolioHandlerTest, MultipleEndpoints_WorkCorrectlyInSequence) {
    {
        SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/portfolio");
        SimpleResponse res;
        portfolioHandler_->handle(req, res);
        EXPECT_EQ(res.getStatus(), 200);
        
        auto jsonResp = parseJsonResponse(res);
        EXPECT_TRUE(jsonResp.contains("cash"));
        EXPECT_TRUE(jsonResp.contains("positions"));
    }
    
    {
        SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/portfolio/positions");
        SimpleResponse res;
        portfolioHandler_->handle(req, res);
        EXPECT_EQ(res.getStatus(), 200);
        
        auto jsonResp = parseJsonResponse(res);
        EXPECT_TRUE(jsonResp.is_array());
    }
    
    {
        SimpleRequest req = createAuthorizedRequest("GET", "/api/v1/portfolio/cash");
        SimpleResponse res;
        portfolioHandler_->handle(req, res);
        EXPECT_EQ(res.getStatus(), 200);
        
        auto jsonResp = parseJsonResponse(res);
        EXPECT_TRUE(jsonResp.contains("available"));
        EXPECT_TRUE(jsonResp.contains("currency"));
    }
}