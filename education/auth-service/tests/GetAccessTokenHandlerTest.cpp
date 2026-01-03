#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/GetAccessTokenHandler.hpp"
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>
#include <nlohmann/json.hpp>

using namespace auth;
using namespace auth::adapters::primary;
using ::testing::Return;
using ::testing::_;

// ============================================================================
// Mocks
// ============================================================================

class MockAuthService : public ports::input::IAuthService {
public:
    MOCK_METHOD(ports::input::RegisterResult, registerUser,
                (const std::string& username, const std::string& email, const std::string& password), (override));
    MOCK_METHOD(ports::input::LoginResult, login,
                (const std::string& username, const std::string& password), (override));
    MOCK_METHOD(bool, logout, (const std::string& sessionToken), (override));
    MOCK_METHOD(ports::input::ValidateResult, validateSessionToken, (const std::string& token), (override));
    MOCK_METHOD(ports::input::ValidateResult, validateAccessToken, (const std::string& token), (override));
    MOCK_METHOD(std::optional<std::string>, createAccessToken,
                (const std::string& sessionToken, const std::string& accountId), (override));
};

class MockAccountService : public ports::input::IAccountService {
public:
    MOCK_METHOD(domain::Account, createAccount,
                (const std::string& userId, const ports::input::CreateAccountRequest& request), (override));
    MOCK_METHOD(std::vector<domain::Account>, getUserAccounts, (const std::string& userId), (override));
    MOCK_METHOD(std::optional<domain::Account>, getAccountById, (const std::string& accountId), (override));
    MOCK_METHOD(bool, deleteAccount, (const std::string& accountId), (override));
    MOCK_METHOD(bool, isAccountOwner, (const std::string& userId, const std::string& accountId), (override));
};

// ============================================================================
// Test Fixture
// ============================================================================

class GetAccessTokenHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockAuthService_ = std::make_shared<MockAuthService>();
        mockAccountService_ = std::make_shared<MockAccountService>();
        handler_ = std::make_unique<GetAccessTokenHandler>(mockAuthService_, mockAccountService_);
    }

    SimpleRequest createRequest(const std::string& sessionToken, const std::string& body) {
        SimpleRequest req("POST", "/api/v1/auth/access-token", body, "127.0.0.1", 8081);
        if (!sessionToken.empty()) {
            req.setHeader("Authorization", "Bearer " + sessionToken);
        }
        return req;
    }

    nlohmann::json parseResponse(const SimpleResponse& res) {
        return nlohmann::json::parse(res.getBody());
    }

    std::shared_ptr<MockAuthService> mockAuthService_;
    std::shared_ptr<MockAccountService> mockAccountService_;
    std::unique_ptr<GetAccessTokenHandler> handler_;
};

// ============================================================================
// ТЕСТЫ: Успешный сценарий
// ============================================================================

TEST_F(GetAccessTokenHandlerTest, Success_ReturnsAccessToken) {
    std::string sessionToken = "valid-session-token";
    std::string accountId = "acc-123";
    std::string userId = "user-001";
    std::string accessToken = "generated-access-token";

    // Session token валиден
    ports::input::ValidateResult validSession{true, userId, "", "Valid"};
    EXPECT_CALL(*mockAuthService_, validateSessionToken(sessionToken))
        .WillOnce(Return(validSession));

    // Account принадлежит user
    EXPECT_CALL(*mockAccountService_, isAccountOwner(userId, accountId))
        .WillOnce(Return(true));

    // Access token создан
    EXPECT_CALL(*mockAuthService_, createAccessToken(sessionToken, accountId))
        .WillOnce(Return(accessToken));

    auto req = createRequest(sessionToken, R"({"account_id": "acc-123"})");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseResponse(res);
    EXPECT_EQ(json["access_token"], accessToken);
    EXPECT_EQ(json["user_id"], userId);
    EXPECT_EQ(json["account_id"], accountId);
    EXPECT_EQ(json["token_type"], "Bearer");
    EXPECT_EQ(json["expires_in"], 3600);
}

// ============================================================================
// ТЕСТЫ: Ошибки авторизации
// ============================================================================

TEST_F(GetAccessTokenHandlerTest, NoAuthHeader_Returns401) {
    auto req = createRequest("", R"({"account_id": "acc-123"})");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
    auto json = parseResponse(res);
    EXPECT_TRUE(json.contains("error"));
}

TEST_F(GetAccessTokenHandlerTest, InvalidSessionToken_Returns401) {
    std::string sessionToken = "invalid-session-token";

    ports::input::ValidateResult invalidSession{false, "", "", "Invalid or expired token"};
    EXPECT_CALL(*mockAuthService_, validateSessionToken(sessionToken))
        .WillOnce(Return(invalidSession));

    auto req = createRequest(sessionToken, R"({"account_id": "acc-123"})");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
    auto json = parseResponse(res);
    EXPECT_EQ(json["error"], "Invalid or expired token");
}

// ============================================================================
// ТЕСТЫ: Ошибки валидации body
// ============================================================================

TEST_F(GetAccessTokenHandlerTest, MissingAccountId_Returns400) {
    std::string sessionToken = "valid-session-token";

    ports::input::ValidateResult validSession{true, "user-001", "", "Valid"};
    EXPECT_CALL(*mockAuthService_, validateSessionToken(sessionToken))
        .WillOnce(Return(validSession));

    auto req = createRequest(sessionToken, R"({})");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);
    auto json = parseResponse(res);
    EXPECT_TRUE(json["error"].get<std::string>().find("account_id") != std::string::npos);
}

TEST_F(GetAccessTokenHandlerTest, EmptyAccountId_Returns400) {
    std::string sessionToken = "valid-session-token";

    ports::input::ValidateResult validSession{true, "user-001", "", "Valid"};
    EXPECT_CALL(*mockAuthService_, validateSessionToken(sessionToken))
        .WillOnce(Return(validSession));

    auto req = createRequest(sessionToken, R"({"account_id": ""})");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);
}

TEST_F(GetAccessTokenHandlerTest, InvalidJson_Returns400) {
    std::string sessionToken = "valid-session-token";

    ports::input::ValidateResult validSession{true, "user-001", "", "Valid"};
    EXPECT_CALL(*mockAuthService_, validateSessionToken(sessionToken))
        .WillOnce(Return(validSession));

    auto req = createRequest(sessionToken, "not-json");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);
    auto json = parseResponse(res);
    EXPECT_EQ(json["error"], "Invalid JSON");
}

// ============================================================================
// ТЕСТЫ: Проверка ownership
// ============================================================================

TEST_F(GetAccessTokenHandlerTest, AccountNotOwned_Returns403) {
    std::string sessionToken = "valid-session-token";
    std::string accountId = "acc-not-mine";
    std::string userId = "user-001";

    ports::input::ValidateResult validSession{true, userId, "", "Valid"};
    EXPECT_CALL(*mockAuthService_, validateSessionToken(sessionToken))
        .WillOnce(Return(validSession));

    // Account НЕ принадлежит user
    EXPECT_CALL(*mockAccountService_, isAccountOwner(userId, accountId))
        .WillOnce(Return(false));

    auto req = createRequest(sessionToken, R"({"account_id": "acc-not-mine"})");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 403);
    auto json = parseResponse(res);
    EXPECT_TRUE(json["error"].get<std::string>().find("does not belong") != std::string::npos);
}

// ============================================================================
// ТЕСТЫ: Ошибка создания токена
// ============================================================================

TEST_F(GetAccessTokenHandlerTest, TokenCreationFails_Returns500) {
    std::string sessionToken = "valid-session-token";
    std::string accountId = "acc-123";
    std::string userId = "user-001";

    ports::input::ValidateResult validSession{true, userId, "", "Valid"};
    EXPECT_CALL(*mockAuthService_, validateSessionToken(sessionToken))
        .WillOnce(Return(validSession));

    EXPECT_CALL(*mockAccountService_, isAccountOwner(userId, accountId))
        .WillOnce(Return(true));

    // Не удалось создать токен
    EXPECT_CALL(*mockAuthService_, createAccessToken(sessionToken, accountId))
        .WillOnce(Return(std::nullopt));

    auto req = createRequest(sessionToken, R"({"account_id": "acc-123"})");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 500);
}
