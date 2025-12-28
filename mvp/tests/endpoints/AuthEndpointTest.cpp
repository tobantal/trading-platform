/**
 * @file AuthEndpointTest.cpp
 * @brief Тесты для Auth endpoints
 * 
 * Тестируемые endpoint-ы:
 * - POST /api/v1/auth/register    → RegisterHandler
 * - POST /api/v1/auth/login       → LoginHandler
 * - POST /api/v1/auth/select-account → SelectAccountHandler
 * - POST /api/v1/auth/validate    → ValidateTokenHandler
 * - POST /api/v1/auth/refresh     → RefreshTokenHandler
 * - POST /api/v1/auth/logout      → LogoutHandler
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

// Handlers
#include "adapters/primary/auth/RegisterHandler.hpp"
#include "adapters/primary/auth/LoginHandler.hpp"
#include "adapters/primary/auth/SelectAccountHandler.hpp"
#include "adapters/primary/auth/ValidateTokenHandler.hpp"
#include "adapters/primary/auth/RefreshTokenHandler.hpp"
#include "adapters/primary/auth/LogoutHandler.hpp"

// Services & Adapters
#include "application/AuthService.hpp"
#include "adapters/secondary/auth/FakeJwtAdapter.hpp"
#include "adapters/secondary/persistence/InMemoryUserRepository.hpp"
#include "adapters/secondary/persistence/InMemoryAccountRepository.hpp"

// Test utilities
#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>

using namespace trading::adapters::primary;
using namespace trading::application;
using namespace trading::adapters::secondary;
using namespace trading::domain;
using json = nlohmann::json;

// ============================================================================
// TEST FIXTURE
// ============================================================================

class AuthHandlersTest : public ::testing::Test {
protected:
    void SetUp() override {
        jwtProvider_ = std::make_shared<FakeJwtAdapter>();
        userRepo_ = std::make_shared<InMemoryUserRepository>();
        accountRepo_ = std::make_shared<InMemoryAccountRepository>();
        
        authService_ = std::make_shared<AuthService>(
            jwtProvider_, userRepo_, accountRepo_
        );
        
        registerHandler_ = std::make_shared<RegisterHandler>(authService_);
        loginHandler_ = std::make_shared<LoginHandler>(authService_);
        selectAccountHandler_ = std::make_shared<SelectAccountHandler>(authService_);
        validateHandler_ = std::make_shared<ValidateTokenHandler>(authService_);
        refreshHandler_ = std::make_shared<RefreshTokenHandler>(authService_);
        logoutHandler_ = std::make_shared<LogoutHandler>(authService_);
    }

    void TearDown() override {
        // Очищаем репозитории
        userRepo_->clear();
        accountRepo_->clear();
    }

    /**
     * @brief Создать HTTP запрос
     */
    SimpleRequest createRequest(
        const std::string& method,
        const std::string& path,
        const std::string& body = "",
        const std::string& authToken = ""
    ) {
        std::map<std::string, std::string> headers;
        headers["Content-Type"] = "application/json";
        
        if (!authToken.empty()) {
            headers["Authorization"] = "Bearer " + authToken;
        }

        return SimpleRequest(method, path, body, "127.0.0.1", 8080, headers);
    }

    /**
     * @brief Распарсить тело ответа как JSON
     */
    json parseResponse(const SimpleResponse& res) {
        return json::parse(res.getBody());
    }

    /**
     * @brief Зарегистрировать пользователя
     */
    std::string registerUser(const std::string& username, const std::string& password) {
        auto req = createRequest(
            "POST", "/api/v1/auth/register",
            json{{"username", username}, {"password", password}}.dump()
        );
        SimpleResponse res;
        registerHandler_->handle(req, res);
        
        if (res.getStatus() != 201) {
            return "";
        }
        
        auto body = parseResponse(res);
        return body.value("user_id", "");
    }

    /**
     * @brief Войти и получить session token
     */
    std::string loginAndGetSessionToken(const std::string& username, const std::string& password) {
        auto req = createRequest(
            "POST", "/api/v1/auth/login",
            json{{"username", username}, {"password", password}}.dump()
        );
        SimpleResponse res;
        loginHandler_->handle(req, res);
        
        if (res.getStatus() != 200) {
            return "";
        }
        
        auto body = parseResponse(res);
        return body.value("session_token", "");
    }

    /**
     * @brief Выбрать аккаунт и получить access token
     */
    std::string selectAccountAndGetAccessToken(
        const std::string& sessionToken,
        const std::string& accountId
    ) {
        auto req = createRequest(
            "POST", "/api/v1/auth/select-account",
            json{{"account_id", accountId}}.dump(),
            sessionToken
        );
        SimpleResponse res;
        selectAccountHandler_->handle(req, res);
        
        if (res.getStatus() != 200) {
            return "";
        }
        
        auto body = parseResponse(res);
        return body.value("access_token", "");
    }

    // Зависимости
    std::shared_ptr<FakeJwtAdapter> jwtProvider_;
    std::shared_ptr<InMemoryUserRepository> userRepo_;
    std::shared_ptr<InMemoryAccountRepository> accountRepo_;
    std::shared_ptr<AuthService> authService_;
    
    // Хэндлеры
    std::shared_ptr<RegisterHandler> registerHandler_;
    std::shared_ptr<LoginHandler> loginHandler_;
    std::shared_ptr<SelectAccountHandler> selectAccountHandler_;
    std::shared_ptr<ValidateTokenHandler> validateHandler_;
    std::shared_ptr<RefreshTokenHandler> refreshHandler_;
    std::shared_ptr<LogoutHandler> logoutHandler_;
};

// ============================================================================
// REGISTER HANDLER TESTS
// ============================================================================

TEST_F(AuthHandlersTest, Register_ValidData_CreatesUser) {
    auto req = createRequest(
        "POST", "/api/v1/auth/register",
        json{{"username", "newuser"}, {"password", "secret123"}}.dump()
    );
    SimpleResponse res;
    
    registerHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 201);
    
    auto body = parseResponse(res);
    EXPECT_TRUE(body.contains("user_id"));
    EXPECT_EQ(body["message"], "User registered successfully");
}

TEST_F(AuthHandlersTest, Register_DuplicateUsername_Returns409) {
    // Используем уникальное имя пользователя
    std::string username = "duplicate_" + std::to_string(rand());
    
    // Регистрируем первого пользователя
    auto user1Req = createRequest(
        "POST", "/api/v1/auth/register",
        json{{"username", username}, {"password", "password1"}}.dump()
    );
    SimpleResponse user1Res;
    registerHandler_->handle(user1Req, user1Res);
    EXPECT_EQ(user1Res.getStatus(), 201);
    
    // Пытаемся зарегистрировать с тем же username
    auto req = createRequest(
        "POST", "/api/v1/auth/register",
        json{{"username", username}, {"password", "password2"}}.dump()
    );
    SimpleResponse res;
    
    registerHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 409);
    
    auto body = parseResponse(res);
    EXPECT_TRUE(body["error"].get<std::string>().find("already exists") != std::string::npos);
}

TEST_F(AuthHandlersTest, Register_InvalidJSON_Returns400) {
    auto req = createRequest(
        "POST", "/api/v1/auth/register",
        "{invalid json"
    );
    SimpleResponse res;
    
    registerHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
}

// ============================================================================
// LOGIN HANDLER TESTS
// ============================================================================

TEST_F(AuthHandlersTest, Login_ValidCredentials_ReturnsSessionToken) {
    // Используем уникальное имя пользователя
    std::string username = "testuser_" + std::to_string(rand());
    std::string password = "password123";
    
    std::cout << "Testing with username: " << username << std::endl;
    
    // Регистрируем пользователя
    auto registerReq = createRequest(
        "POST", "/api/v1/auth/register",
        json{{"username", username}, {"password", password}}.dump()
    );
    SimpleResponse registerRes;
    registerHandler_->handle(registerReq, registerRes);
    
    std::cout << "Register status: " << registerRes.getStatus() << std::endl;
    std::cout << "Register body: " << registerRes.getBody() << std::endl;
    
    EXPECT_EQ(registerRes.getStatus(), 201);
    
    // Входим
    auto req = createRequest(
        "POST", "/api/v1/auth/login",
        json{{"username", username}, {"password", password}}.dump()
    );
    SimpleResponse res;
    
    loginHandler_->handle(req, res);
    
    std::cout << "Login status: " << res.getStatus() << std::endl;
    std::cout << "Login body: " << res.getBody() << std::endl;
    
    EXPECT_EQ(res.getStatus(), 200);
    
    if (res.getStatus() == 200) {
        auto body = parseResponse(res);
        EXPECT_TRUE(body.contains("session_token"));
        EXPECT_EQ(body["token_type"], "Bearer");
        EXPECT_EQ(body["expires_in"], 86400);
        EXPECT_TRUE(body.contains("user"));
        EXPECT_TRUE(body.contains("accounts"));
        EXPECT_TRUE(body["accounts"].is_array());
    }
}

// ============================================================================
// SELECT ACCOUNT HANDLER TESTS
// ============================================================================

TEST_F(AuthHandlersTest, SelectAccount_NoToken_Returns401) {
    auto req = createRequest(
        "POST", "/api/v1/auth/select-account",
        json{{"account_id", "acc-001"}}.dump()
    );
    SimpleResponse res;
    
    selectAccountHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 401);
}

// ============================================================================
// VALIDATE TOKEN HANDLER TESTS
// ============================================================================

TEST_F(AuthHandlersTest, ValidateToken_InvalidToken_ReturnsInvalid) {
    auto req = createRequest(
        "POST", "/api/v1/auth/validate",
        json{{"token", "invalid.token.here"}}.dump()
    );
    SimpleResponse res;
    
    validateHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);  // Всегда 200
    
    auto body = parseResponse(res);
    EXPECT_FALSE(body["valid"]);
    EXPECT_TRUE(body.contains("error"));
}

// ============================================================================
// INTEGRATION TEST
// ============================================================================

TEST_F(AuthHandlersTest, FullFlow_RegisterLogin) {
    // 1. Register
    auto registerReq = createRequest(
        "POST", "/api/v1/auth/register",
        json{{"username", "fullflow"}, {"password", "password123"}}.dump()
    );
    SimpleResponse registerRes;
    registerHandler_->handle(registerReq, registerRes);
    EXPECT_EQ(registerRes.getStatus(), 201);
    
    auto registerBody = parseResponse(registerRes);
    std::string userId = registerBody["user_id"];
    EXPECT_FALSE(userId.empty());
    
    // 2. Login
    auto loginReq = createRequest(
        "POST", "/api/v1/auth/login",
        json{{"username", "fullflow"}, {"password", "password123"}}.dump()
    );
    SimpleResponse loginRes;
    loginHandler_->handle(loginReq, loginRes);
    EXPECT_EQ(loginRes.getStatus(), 200);
    
    auto loginBody = parseResponse(loginRes);
    std::string sessionToken = loginBody["session_token"];
    EXPECT_FALSE(sessionToken.empty());
}
