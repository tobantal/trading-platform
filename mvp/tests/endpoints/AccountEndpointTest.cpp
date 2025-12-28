/**
 * @file AccountEndpointTest.cpp
 * @brief Тесты для Account endpoints
 * 
 * Тестируемые endpoint-ы:
 * - GET /api/v1/accounts           → GetAccountsHandler
 * - POST /api/v1/accounts          → AddAccountHandler
 * - DELETE /api/v1/accounts/{id}   → DeleteAccountHandler
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

// Account Handlers
#include "adapters/primary/account/GetAccountsHandler.hpp"
#include "adapters/primary/account/AddAccountHandler.hpp"
#include "adapters/primary/account/DeleteAccountHandler.hpp"

// Auth Handlers (для получения токенов)
#include "adapters/primary/auth/RegisterHandler.hpp"
#include "adapters/primary/auth/LoginHandler.hpp"

// Services & Adapters
#include "application/AuthService.hpp"
#include "application/AccountService.hpp"
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

class AccountHandlersTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Secondary adapters
        jwtProvider_ = std::make_shared<FakeJwtAdapter>();
        userRepo_ = std::make_shared<InMemoryUserRepository>();
        accountRepo_ = std::make_shared<InMemoryAccountRepository>();
        
        // Services
        authService_ = std::make_shared<AuthService>(
            jwtProvider_, userRepo_, accountRepo_
        );
        accountService_ = std::make_shared<AccountService>(accountRepo_);
        
        // Auth handlers (для setup)
        registerHandler_ = std::make_shared<RegisterHandler>(authService_);
        loginHandler_ = std::make_shared<LoginHandler>(authService_);
        
        // Account handlers (тестируемые)
        getAccountsHandler_ = std::make_shared<GetAccountsHandler>(
            authService_, accountService_
        );
        addAccountHandler_ = std::make_shared<AddAccountHandler>(
            authService_, accountService_
        );
        deleteAccountHandler_ = std::make_shared<DeleteAccountHandler>(
            authService_, accountService_
        );
    }

    void TearDown() override {
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
     * @brief Зарегистрировать пользователя и вернуть session token
     */
    std::string registerAndLogin(const std::string& username) {
        // Register
        auto regReq = createRequest(
            "POST", "/api/v1/auth/register",
            json{{"username", username}, {"password", "password123"}}.dump()
        );
        SimpleResponse regRes;
        registerHandler_->handle(regReq, regRes);
        
        if (regRes.getStatus() != 201) {
            return "";
        }
        
        // Login
        auto loginReq = createRequest(
            "POST", "/api/v1/auth/login",
            json{{"username", username}, {"password", "password123"}}.dump()
        );
        SimpleResponse loginRes;
        loginHandler_->handle(loginReq, loginRes);
        
        if (loginRes.getStatus() != 200) {
            return "";
        }
        
        auto body = parseResponse(loginRes);
        return body.value("session_token", "");
    }

    // Dependencies
    std::shared_ptr<FakeJwtAdapter> jwtProvider_;
    std::shared_ptr<InMemoryUserRepository> userRepo_;
    std::shared_ptr<InMemoryAccountRepository> accountRepo_;
    std::shared_ptr<AuthService> authService_;
    std::shared_ptr<AccountService> accountService_;
    
    // Handlers
    std::shared_ptr<RegisterHandler> registerHandler_;
    std::shared_ptr<LoginHandler> loginHandler_;
    std::shared_ptr<GetAccountsHandler> getAccountsHandler_;
    std::shared_ptr<AddAccountHandler> addAccountHandler_;
    std::shared_ptr<DeleteAccountHandler> deleteAccountHandler_;
};

// ============================================================================
// GET ACCOUNTS HANDLER TESTS
// ============================================================================

TEST_F(AccountHandlersTest, GetAccounts_NoToken_Returns401) {
    auto req = createRequest("GET", "/api/v1/accounts");
    SimpleResponse res;
    
    getAccountsHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 401);
}

TEST_F(AccountHandlersTest, GetAccounts_InvalidToken_Returns401) {
    auto req = createRequest("GET", "/api/v1/accounts", "", "invalid.token.here");
    SimpleResponse res;
    
    getAccountsHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 401);
}

TEST_F(AccountHandlersTest, GetAccounts_ValidToken_EmptyList) {
    // Регистрируемся и логинимся
    std::string sessionToken = registerAndLogin("test_user_empty_" + std::to_string(rand()));
    ASSERT_FALSE(sessionToken.empty());
    
    // Получаем аккаунты (должен быть пустой список)
    auto req = createRequest("GET", "/api/v1/accounts", "", sessionToken);
    SimpleResponse res;
    
    getAccountsHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto body = parseResponse(res);
    EXPECT_TRUE(body.contains("accounts"));
    EXPECT_TRUE(body["accounts"].is_array());
    EXPECT_TRUE(body["accounts"].empty());
}

TEST_F(AccountHandlersTest, GetAccounts_ValidToken_WithAccounts) {
    std::string username = "test_user_with_acc_" + std::to_string(rand());
    std::string sessionToken = registerAndLogin(username);
    ASSERT_FALSE(sessionToken.empty());
    
    // Добавляем аккаунт
    auto addReq = createRequest(
        "POST", "/api/v1/accounts",
        json{
            {"name", "Test Account"},
            {"type", "SANDBOX"},
            {"broker_token", "t.test-token"}
        }.dump(),
        sessionToken
    );
    SimpleResponse addRes;
    addAccountHandler_->handle(addReq, addRes);
    EXPECT_EQ(addRes.getStatus(), 201);
    
    // Получаем список аккаунтов
    auto req = createRequest("GET", "/api/v1/accounts", "", sessionToken);
    SimpleResponse res;
    
    getAccountsHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto body = parseResponse(res);
    EXPECT_TRUE(body.contains("accounts"));
    EXPECT_EQ(body["accounts"].size(), 1);
    EXPECT_EQ(body["accounts"][0]["name"], "Test Account");
    EXPECT_EQ(body["accounts"][0]["type"], "SANDBOX");
}

// ============================================================================
// ADD ACCOUNT HANDLER TESTS
// ============================================================================

TEST_F(AccountHandlersTest, AddAccount_NoToken_Returns401) {
    auto req = createRequest(
        "POST", "/api/v1/accounts",
        json{
            {"name", "Test"},
            {"type", "SANDBOX"},
            {"broker_token", "token"}
        }.dump()
    );
    SimpleResponse res;
    
    addAccountHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 401);
}

TEST_F(AccountHandlersTest, AddAccount_ValidData_CreatesAccount) {
    std::string sessionToken = registerAndLogin("add_acc_user_" + std::to_string(rand()));
    ASSERT_FALSE(sessionToken.empty());
    
    auto req = createRequest(
        "POST", "/api/v1/accounts",
        json{
            {"name", "My Sandbox"},
            {"type", "SANDBOX"},
            {"broker_token", "t.sandbox-token-123"}
        }.dump(),
        sessionToken
    );
    SimpleResponse res;
    
    addAccountHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 201);
    
    auto body = parseResponse(res);
    EXPECT_TRUE(body.contains("account"));
    EXPECT_TRUE(body["account"].contains("id"));
    EXPECT_EQ(body["account"]["name"], "My Sandbox");
    EXPECT_EQ(body["account"]["type"], "SANDBOX");
    EXPECT_EQ(body["account"]["active"], true);
}

TEST_F(AccountHandlersTest, AddAccount_ProductionType_CreatesAccount) {
    std::string sessionToken = registerAndLogin("add_prod_user_" + std::to_string(rand()));
    ASSERT_FALSE(sessionToken.empty());
    
    auto req = createRequest(
        "POST", "/api/v1/accounts",
        json{
            {"name", "Real Trading"},
            {"type", "PRODUCTION"},
            {"broker_token", "t.real-token-456"}
        }.dump(),
        sessionToken
    );
    SimpleResponse res;
    
    addAccountHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 201);
    
    auto body = parseResponse(res);
    EXPECT_EQ(body["account"]["type"], "PRODUCTION");
}

TEST_F(AccountHandlersTest, AddAccount_MissingName_Returns400) {
    std::string sessionToken = registerAndLogin("missing_name_" + std::to_string(rand()));
    ASSERT_FALSE(sessionToken.empty());
    
    auto req = createRequest(
        "POST", "/api/v1/accounts",
        json{
            {"type", "SANDBOX"},
            {"broker_token", "token"}
        }.dump(),
        sessionToken
    );
    SimpleResponse res;
    
    addAccountHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
    
    auto body = parseResponse(res);
    EXPECT_TRUE(body["error"].get<std::string>().find("name") != std::string::npos);
}

TEST_F(AccountHandlersTest, AddAccount_MissingType_Returns400) {
    std::string sessionToken = registerAndLogin("missing_type_" + std::to_string(rand()));
    ASSERT_FALSE(sessionToken.empty());
    
    auto req = createRequest(
        "POST", "/api/v1/accounts",
        json{
            {"name", "Test"},
            {"broker_token", "token"}
        }.dump(),
        sessionToken
    );
    SimpleResponse res;
    
    addAccountHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
}

TEST_F(AccountHandlersTest, AddAccount_MissingBrokerToken_Returns400) {
    std::string sessionToken = registerAndLogin("missing_broker_" + std::to_string(rand()));
    ASSERT_FALSE(sessionToken.empty());
    
    auto req = createRequest(
        "POST", "/api/v1/accounts",
        json{
            {"name", "Test"},
            {"type", "SANDBOX"}
        }.dump(),
        sessionToken
    );
    SimpleResponse res;
    
    addAccountHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
}

TEST_F(AccountHandlersTest, AddAccount_InvalidType_Returns422) {
    std::string sessionToken = registerAndLogin("invalid_type_" + std::to_string(rand()));
    ASSERT_FALSE(sessionToken.empty());
    
    auto req = createRequest(
        "POST", "/api/v1/accounts",
        json{
            {"name", "Test"},
            {"type", "INVALID_TYPE"},
            {"broker_token", "token"}
        }.dump(),
        sessionToken
    );
    SimpleResponse res;
    
    addAccountHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 422);
}

TEST_F(AccountHandlersTest, AddAccount_InvalidJson_Returns400) {
    std::string sessionToken = registerAndLogin("invalid_json_" + std::to_string(rand()));
    ASSERT_FALSE(sessionToken.empty());
    
    auto req = createRequest(
        "POST", "/api/v1/accounts",
        "{invalid json",
        sessionToken
    );
    SimpleResponse res;
    
    addAccountHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
}

// ============================================================================
// DELETE ACCOUNT HANDLER TESTS
// ============================================================================

TEST_F(AccountHandlersTest, DeleteAccount_NoToken_Returns401) {
    auto req = createRequest("DELETE", "/api/v1/accounts/acc-123");
    SimpleResponse res;
    
    deleteAccountHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 401);
}

TEST_F(AccountHandlersTest, DeleteAccount_NotFound_Returns404) {
    std::string sessionToken = registerAndLogin("delete_nf_" + std::to_string(rand()));
    ASSERT_FALSE(sessionToken.empty());
    
    auto req = createRequest(
        "DELETE", "/api/v1/accounts/non-existent-account",
        "",
        sessionToken
    );
    SimpleResponse res;
    
    deleteAccountHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
}

TEST_F(AccountHandlersTest, DeleteAccount_Success) {
    std::string sessionToken = registerAndLogin("delete_ok_" + std::to_string(rand()));
    ASSERT_FALSE(sessionToken.empty());
    
    // Создаём аккаунт
    auto addReq = createRequest(
        "POST", "/api/v1/accounts",
        json{
            {"name", "To Delete"},
            {"type", "SANDBOX"},
            {"broker_token", "token"}
        }.dump(),
        sessionToken
    );
    SimpleResponse addRes;
    addAccountHandler_->handle(addReq, addRes);
    EXPECT_EQ(addRes.getStatus(), 201);
    
    auto addBody = parseResponse(addRes);
    std::string accountId = addBody["account"]["id"];
    
    // Удаляем аккаунт
    auto req = createRequest(
        "DELETE", "/api/v1/accounts/" + accountId,
        "",
        sessionToken
    );
    SimpleResponse res;
    
    deleteAccountHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto body = parseResponse(res);
    EXPECT_TRUE(body["message"].get<std::string>().find("deleted") != std::string::npos);
    
    // Проверяем, что аккаунт удалён
    auto getReq = createRequest("GET", "/api/v1/accounts", "", sessionToken);
    SimpleResponse getRes;
    getAccountsHandler_->handle(getReq, getRes);
    
    auto getBody = parseResponse(getRes);
    EXPECT_TRUE(getBody["accounts"].empty());
}

TEST_F(AccountHandlersTest, DeleteAccount_OtherUserAccount_Returns403) {
    // Создаём первого пользователя с аккаунтом
    std::string user1Token = registerAndLogin("user1_own_" + std::to_string(rand()));
    ASSERT_FALSE(user1Token.empty());
    
    auto addReq = createRequest(
        "POST", "/api/v1/accounts",
        json{
            {"name", "User1 Account"},
            {"type", "SANDBOX"},
            {"broker_token", "token1"}
        }.dump(),
        user1Token
    );
    SimpleResponse addRes;
    addAccountHandler_->handle(addReq, addRes);
    EXPECT_EQ(addRes.getStatus(), 201);
    
    auto addBody = parseResponse(addRes);
    std::string user1AccountId = addBody["account"]["id"];
    
    // Создаём второго пользователя
    std::string user2Token = registerAndLogin("user2_other_" + std::to_string(rand()));
    ASSERT_FALSE(user2Token.empty());
    
    // Пытаемся удалить аккаунт первого пользователя от имени второго
    auto req = createRequest(
        "DELETE", "/api/v1/accounts/" + user1AccountId,
        "",
        user2Token
    );
    SimpleResponse res;
    
    deleteAccountHandler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 403);
}

// ============================================================================
// INTEGRATION TEST: FULL ACCOUNT FLOW
// ============================================================================

TEST_F(AccountHandlersTest, FullFlow_CreateListDelete) {
    std::string sessionToken = registerAndLogin("flow_user_" + std::to_string(rand()));
    ASSERT_FALSE(sessionToken.empty());
    
    // 1. Изначально аккаунтов нет
    {
        auto req = createRequest("GET", "/api/v1/accounts", "", sessionToken);
        SimpleResponse res;
        getAccountsHandler_->handle(req, res);
        
        EXPECT_EQ(res.getStatus(), 200);
        auto body = parseResponse(res);
        EXPECT_TRUE(body["accounts"].empty());
    }
    
    // 2. Добавляем sandbox аккаунт
    std::string sandboxId;
    {
        auto req = createRequest(
            "POST", "/api/v1/accounts",
            json{
                {"name", "Sandbox"},
                {"type", "SANDBOX"},
                {"broker_token", "t.sandbox"}
            }.dump(),
            sessionToken
        );
        SimpleResponse res;
        addAccountHandler_->handle(req, res);
        
        EXPECT_EQ(res.getStatus(), 201);
        auto body = parseResponse(res);
        sandboxId = body["account"]["id"];
        EXPECT_FALSE(sandboxId.empty());
    }
    
    // 3. Добавляем production аккаунт
    std::string productionId;
    {
        auto req = createRequest(
            "POST", "/api/v1/accounts",
            json{
                {"name", "Production"},
                {"type", "PRODUCTION"},
                {"broker_token", "t.production"}
            }.dump(),
            sessionToken
        );
        SimpleResponse res;
        addAccountHandler_->handle(req, res);
        
        EXPECT_EQ(res.getStatus(), 201);
        auto body = parseResponse(res);
        productionId = body["account"]["id"];
    }
    
    // 4. Проверяем, что оба аккаунта в списке
    {
        auto req = createRequest("GET", "/api/v1/accounts", "", sessionToken);
        SimpleResponse res;
        getAccountsHandler_->handle(req, res);
        
        EXPECT_EQ(res.getStatus(), 200);
        auto body = parseResponse(res);
        EXPECT_EQ(body["accounts"].size(), 2);
    }
    
    // 5. Удаляем sandbox аккаунт
    {
        auto req = createRequest(
            "DELETE", "/api/v1/accounts/" + sandboxId,
            "",
            sessionToken
        );
        SimpleResponse res;
        deleteAccountHandler_->handle(req, res);
        
        EXPECT_EQ(res.getStatus(), 200);
    }
    
    // 6. Проверяем, что остался только production
    {
        auto req = createRequest("GET", "/api/v1/accounts", "", sessionToken);
        SimpleResponse res;
        getAccountsHandler_->handle(req, res);
        
        EXPECT_EQ(res.getStatus(), 200);
        auto body = parseResponse(res);
        EXPECT_EQ(body["accounts"].size(), 1);
        EXPECT_EQ(body["accounts"][0]["id"], productionId);
        EXPECT_EQ(body["accounts"][0]["type"], "PRODUCTION");
    }
}
