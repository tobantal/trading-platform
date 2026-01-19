#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/RegisterHandler.hpp"
#include "adapters/primary/LoginHandler.hpp"
#include "adapters/primary/ValidateTokenHandler.hpp"
#include "adapters/primary/GetAccountsHandler.hpp"
#include "adapters/primary/AddAccountHandler.hpp"

#include "application/AuthService.hpp"
#include "application/AccountService.hpp"
#include "adapters/secondary/FakeJwtAdapter.hpp"
#include "adapters/secondary/AuthSettings.hpp"
#include "mocks/InMemoryUserRepository.hpp"
#include "mocks/InMemoryAccountRepository.hpp"
#include "mocks/InMemorySessionRepository.hpp"

// Используем SimpleRequest/SimpleResponse из http-server-core
#include "SimpleRequest.hpp"
#include "SimpleResponse.hpp"

#include <nlohmann/json.hpp>

using namespace auth;
using namespace auth::tests::mocks;
using namespace auth::adapters::primary;

// ============================================
// TEST FIXTURE
// ============================================

class AuthEndpointTest : public ::testing::Test {
protected:
    void SetUp() override {
        settings_ = std::make_shared<adapters::secondary::AuthSettings>();
        userRepo_ = std::make_shared<InMemoryUserRepository>();
        accountRepo_ = std::make_shared<InMemoryAccountRepository>();
        sessionRepo_ = std::make_shared<InMemorySessionRepository>();
        jwtProvider_ = std::make_shared<adapters::secondary::FakeJwtAdapter>();
        
        authService_ = std::make_shared<application::AuthService>(
            settings_, userRepo_, sessionRepo_, jwtProvider_
        );
        accountService_ = std::make_shared<application::AccountService>(accountRepo_);
    }

    void TearDown() override {
        userRepo_->clear();
        accountRepo_->clear();
        sessionRepo_->clear();
    }

    std::shared_ptr<adapters::secondary::AuthSettings> settings_;
    std::shared_ptr<InMemoryUserRepository> userRepo_;
    std::shared_ptr<InMemoryAccountRepository> accountRepo_;
    std::shared_ptr<InMemorySessionRepository> sessionRepo_;
    std::shared_ptr<adapters::secondary::FakeJwtAdapter> jwtProvider_;
    std::shared_ptr<application::AuthService> authService_;
    std::shared_ptr<application::AccountService> accountService_;
};

// ============================================
// REGISTER HANDLER TESTS
// ============================================

TEST_F(AuthEndpointTest, RegisterHandler_Success) {
    RegisterHandler handler(authService_);
    
    SimpleRequest req;
    req.setMethod("POST");
    req.setPath("/api/v1/auth/register");
    req.setBody(R"({"username": "john", "email": "john@test.com", "password": "pass123"})");
    
    SimpleResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.getStatus(), 201);
    
    auto json = nlohmann::json::parse(res.getBody());
    EXPECT_FALSE(json["user_id"].get<std::string>().empty());
    EXPECT_EQ(json["message"], "User registered successfully");
}

TEST_F(AuthEndpointTest, RegisterHandler_MissingFields) {
    RegisterHandler handler(authService_);
    
    SimpleRequest req;
    req.setMethod("POST");
    req.setPath("/api/v1/auth/register");
    req.setBody(R"({"username": "john"})");
    
    SimpleResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);
}

TEST_F(AuthEndpointTest, RegisterHandler_InvalidJson) {
    RegisterHandler handler(authService_);
    
    SimpleRequest req;
    req.setMethod("POST");
    req.setPath("/api/v1/auth/register");
    req.setBody("not json");
    
    SimpleResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);
}

// ============================================
// LOGIN HANDLER TESTS
// ============================================

TEST_F(AuthEndpointTest, LoginHandler_Success) {
    // Register first
    authService_->registerUser("john", "john@test.com", "pass123");
    
    LoginHandler handler(authService_);
    
    SimpleRequest req;
    req.setMethod("POST");
    req.setPath("/api/v1/auth/login");
    req.setBody(R"({"username": "john", "password": "pass123"})");
    
    SimpleResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = nlohmann::json::parse(res.getBody());
    EXPECT_FALSE(json["session_token"].get<std::string>().empty());
    EXPECT_FALSE(json["user_id"].get<std::string>().empty());
}

TEST_F(AuthEndpointTest, LoginHandler_WrongPassword) {
    authService_->registerUser("john", "john@test.com", "pass123");
    
    LoginHandler handler(authService_);
    
    SimpleRequest req;
    req.setMethod("POST");
    req.setPath("/api/v1/auth/login");
    req.setBody(R"({"username": "john", "password": "wrongpass"})");
    
    SimpleResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
}

// ============================================
// VALIDATE TOKEN HANDLER TESTS
// ============================================

TEST_F(AuthEndpointTest, ValidateTokenHandler_ValidSession) {
    authService_->registerUser("john", "john@test.com", "pass123");
    auto loginResult = authService_->login("john", "pass123");
    
    ValidateTokenHandler handler(authService_);
    
    SimpleRequest req;
    req.setMethod("POST");
    req.setPath("/api/v1/auth/validate");
    req.setBody(R"({"token": ")" + loginResult.sessionToken + R"(", "type": "session"})");
    
    SimpleResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = nlohmann::json::parse(res.getBody());
    EXPECT_TRUE(json["valid"].get<bool>());
    EXPECT_EQ(json["user_id"], loginResult.userId);
}

TEST_F(AuthEndpointTest, ValidateTokenHandler_InvalidToken) {
    ValidateTokenHandler handler(authService_);
    
    SimpleRequest req;
    req.setMethod("POST");
    req.setPath("/api/v1/auth/validate");
    req.setBody(R"({"token": "invalid-token"})");
    
    SimpleResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = nlohmann::json::parse(res.getBody());
    EXPECT_FALSE(json["valid"].get<bool>());
}

// ============================================
// GET ACCOUNTS HANDLER TESTS
// ============================================

TEST_F(AuthEndpointTest, GetAccountsHandler_Success) {
    authService_->registerUser("john", "john@test.com", "pass123");
    auto loginResult = authService_->login("john", "pass123");
    
    // Create account
    ports::input::CreateAccountRequest accReq{.name = "Test", .type = domain::AccountType::SANDBOX};
    accountService_->createAccount(loginResult.userId, accReq);
    
    GetAccountsHandler handler(authService_, accountService_);
    
    SimpleRequest req;
    req.setMethod("GET");
    req.setPath("/api/v1/accounts");
    req.setHeader("Authorization", "Bearer " + loginResult.sessionToken);
    
    SimpleResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = nlohmann::json::parse(res.getBody());
    EXPECT_EQ(json["accounts"].size(), 1);
    EXPECT_EQ(json["accounts"][0]["name"], "Test");
}

TEST_F(AuthEndpointTest, GetAccountsHandler_Unauthorized) {
    GetAccountsHandler handler(authService_, accountService_);
    
    SimpleRequest req;
    req.setMethod("GET");
    req.setPath("/api/v1/accounts");
    // No Authorization header
    
    SimpleResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
}

// ============================================
// ADD ACCOUNT HANDLER TESTS
// ============================================

TEST_F(AuthEndpointTest, AddAccountHandler_Success) {
    authService_->registerUser("john", "john@test.com", "pass123");
    auto loginResult = authService_->login("john", "pass123");
    
    AddAccountHandler handler(authService_, accountService_);
    
    SimpleRequest req;
    req.setMethod("POST");
    req.setPath("/api/v1/accounts");
    req.setHeader("Authorization", "Bearer " + loginResult.sessionToken);
    req.setHeader("Content-Type", "application/json");
    req.setBody(R"({"name": "My Sandbox", "type": "SANDBOX", "tinkoff_token": "fake-token"})");
    
    SimpleResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.getStatus(), 201);
    
    auto json = nlohmann::json::parse(res.getBody());
    EXPECT_EQ(json["name"], "My Sandbox");
    EXPECT_EQ(json["type"], "SANDBOX");
}

// ============================================
// ДОПОЛНИТЕЛЬНЫЕ ТЕСТЫ С НОВЫМ API v0.1.0
// ============================================

TEST_F(AuthEndpointTest, GetAccountsHandler_VerifyBearerTokenExtraction) {
    // Этот тест демонстрирует использование нового getBearerToken()
    authService_->registerUser("john", "john@test.com", "pass123");
    auto loginResult = authService_->login("john", "pass123");
    
    SimpleRequest req;
    req.setHeader("Authorization", "Bearer " + loginResult.sessionToken);
    
    // Проверяем, что getBearerToken() корректно извлекает токен
    auto token = req.getBearerToken();
    ASSERT_TRUE(token.has_value());
    EXPECT_EQ(*token, loginResult.sessionToken);
}

TEST_F(AuthEndpointTest, AddAccountHandler_VerifyJsonContentType) {
    // Этот тест демонстрирует использование нового isJson()
    SimpleRequest req;
    req.setHeader("Content-Type", "application/json");
    req.setBody(R"({"name": "Test"})");
    
    EXPECT_TRUE(req.isJson());
    
    // Также работает с charset
    req.setHeader("Content-Type", "application/json; charset=utf-8");
    EXPECT_TRUE(req.isJson());
}

TEST_F(AuthEndpointTest, Response_UseSetResult) {
    // Демонстрация нового convenience метода setResult()
    SimpleResponse res;
    
    res.setResult(201, "application/json", R"({"status": "created"})");
    
    EXPECT_EQ(res.getStatus(), 201);
    EXPECT_EQ(res.getBody(), R"({"status": "created"})");
    
    auto contentType = res.getHeader("Content-Type");
    ASSERT_TRUE(contentType.has_value());
    EXPECT_EQ(*contentType, "application/json");
}

TEST_F(AuthEndpointTest, Request_CaseInsensitiveHeaders) {
    // Демонстрация case-insensitive доступа к заголовкам
    SimpleRequest req;
    req.setHeader("Authorization", "Bearer token123");
    
    // Все варианты должны работать
    auto h1 = req.getHeader("Authorization");
    auto h2 = req.getHeader("authorization");
    auto h3 = req.getHeader("AUTHORIZATION");
    
    ASSERT_TRUE(h1.has_value());
    ASSERT_TRUE(h2.has_value());
    ASSERT_TRUE(h3.has_value());
    
    EXPECT_EQ(*h1, "Bearer token123");
    EXPECT_EQ(*h2, "Bearer token123");
    EXPECT_EQ(*h3, "Bearer token123");
}
