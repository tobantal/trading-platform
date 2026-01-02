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

#include <nlohmann/json.hpp>

using namespace auth;
using namespace auth::tests::mocks;
using namespace auth::adapters::primary;

// ============================================
// MOCK REQUEST/RESPONSE
// ============================================

class MockRequest : public IRequest {
public:
    std::string method_;
    std::string path_;
    std::string body_;
    std::map<std::string, std::string> headers_;
    std::map<std::string, std::string> params_;
    std::string ip_ = "127.0.0.1";
    int port_ = 80;

    std::string getMethod() const override { return method_; }
    std::string getPath() const override { return path_; }
    std::string getBody() const override { return body_; }
    std::map<std::string, std::string> getHeaders() const override { return headers_; }
    std::map<std::string, std::string> getParams() const override { return params_; }
    std::string getIp() const override { return ip_; }
    int getPort() const override { return port_; }
};

class MockResponse : public IResponse {
public:
    int status_ = 200;
    std::string body_;
    std::map<std::string, std::string> headers_;

    void setStatus(int status) override { status_ = status; }
    void setHeader(const std::string& key, const std::string& value) override { headers_[key] = value; }
    void setBody(const std::string& body) override { body_ = body; }
    
    // Helpers для тестов (не часть интерфейса)
    int getStatus() const { return status_; }
    std::string getBody() const { return body_; }
};

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
    
    MockRequest req;
    req.method_ = "POST";
    req.path_ = "/api/v1/auth/register";
    req.body_ = R"({"username": "john", "email": "john@test.com", "password": "pass123"})";
    
    MockResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.status_, 201);
    
    auto json = nlohmann::json::parse(res.body_);
    EXPECT_FALSE(json["user_id"].get<std::string>().empty());
    EXPECT_EQ(json["message"], "User registered successfully");
}

TEST_F(AuthEndpointTest, RegisterHandler_MissingFields) {
    RegisterHandler handler(authService_);
    
    MockRequest req;
    req.body_ = R"({"username": "john"})";
    
    MockResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.status_, 400);
}

TEST_F(AuthEndpointTest, RegisterHandler_InvalidJson) {
    RegisterHandler handler(authService_);
    
    MockRequest req;
    req.body_ = "not json";
    
    MockResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.status_, 400);
}

// ============================================
// LOGIN HANDLER TESTS
// ============================================

TEST_F(AuthEndpointTest, LoginHandler_Success) {
    // Register first
    authService_->registerUser("john", "john@test.com", "pass123");
    
    LoginHandler handler(authService_);
    
    MockRequest req;
    req.body_ = R"({"username": "john", "password": "pass123"})";
    
    MockResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.status_, 200);
    
    auto json = nlohmann::json::parse(res.body_);
    EXPECT_FALSE(json["session_token"].get<std::string>().empty());
    EXPECT_FALSE(json["user_id"].get<std::string>().empty());
}

TEST_F(AuthEndpointTest, LoginHandler_WrongPassword) {
    authService_->registerUser("john", "john@test.com", "pass123");
    
    LoginHandler handler(authService_);
    
    MockRequest req;
    req.body_ = R"({"username": "john", "password": "wrongpass"})";
    
    MockResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.status_, 401);
}

// ============================================
// VALIDATE TOKEN HANDLER TESTS
// ============================================

TEST_F(AuthEndpointTest, ValidateTokenHandler_ValidSession) {
    authService_->registerUser("john", "john@test.com", "pass123");
    auto loginResult = authService_->login("john", "pass123");
    
    ValidateTokenHandler handler(authService_);
    
    MockRequest req;
    req.body_ = R"({"token": ")" + loginResult.sessionToken + R"(", "type": "session"})";
    
    MockResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.status_, 200);
    
    auto json = nlohmann::json::parse(res.body_);
    EXPECT_TRUE(json["valid"].get<bool>());
    EXPECT_EQ(json["user_id"], loginResult.userId);
}

TEST_F(AuthEndpointTest, ValidateTokenHandler_InvalidToken) {
    ValidateTokenHandler handler(authService_);
    
    MockRequest req;
    req.body_ = R"({"token": "invalid-token"})";
    
    MockResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.status_, 200);
    
    auto json = nlohmann::json::parse(res.body_);
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
    
    MockRequest req;
    req.headers_["Authorization"] = "Bearer " + loginResult.sessionToken;
    
    MockResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.status_, 200);
    
    auto json = nlohmann::json::parse(res.body_);
    EXPECT_EQ(json["accounts"].size(), 1);
    EXPECT_EQ(json["accounts"][0]["name"], "Test");
}

TEST_F(AuthEndpointTest, GetAccountsHandler_Unauthorized) {
    GetAccountsHandler handler(authService_, accountService_);
    
    MockRequest req;
    // No Authorization header
    
    MockResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.status_, 401);
}

// ============================================
// ADD ACCOUNT HANDLER TESTS
// ============================================

TEST_F(AuthEndpointTest, AddAccountHandler_Success) {
    authService_->registerUser("john", "john@test.com", "pass123");
    auto loginResult = authService_->login("john", "pass123");
    
    AddAccountHandler handler(authService_, accountService_);
    
    MockRequest req;
    req.headers_["Authorization"] = "Bearer " + loginResult.sessionToken;
    req.body_ = R"({"name": "My Sandbox", "type": "SANDBOX", "tinkoff_token": "fake-token"})";
    
    MockResponse res;
    handler.handle(req, res);

    EXPECT_EQ(res.status_, 201);
    
    auto json = nlohmann::json::parse(res.body_);
    EXPECT_EQ(json["name"], "My Sandbox");
    EXPECT_EQ(json["type"], "SANDBOX");
}
