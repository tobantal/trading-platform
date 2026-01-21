// tests/middleware/AccountIdExtractorMiddlewareTest.cpp
/**
 * @file AccountIdExtractorMiddlewareTest.cpp
 * @brief Unit-тесты для AccountIdExtractorMiddleware
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/AccountIdExtractorMiddleware.hpp"
#include "ports/output/IAuthClient.hpp"

#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>
#include <nlohmann/json.hpp>

using namespace trading;
using namespace trading::adapters::primary;
using ::testing::Return;

// ============================================================================
// Mock
// ============================================================================

class MockAuthClient : public ports::output::IAuthClient
{
public:
    MOCK_METHOD(ports::output::TokenValidationResult, validateAccessToken, (const std::string &), (override));
    MOCK_METHOD(std::optional<std::string>, getAccountIdFromToken, (const std::string &), (override));
};

// ============================================================================
// Test Fixture
// ============================================================================

class AccountIdExtractorMiddlewareTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockAuthClient_ = std::make_shared<MockAuthClient>();
        middleware_ = std::make_unique<AccountIdExtractorMiddleware>(mockAuthClient_);
    }

    SimpleRequest createRequest(const std::string &token = "")
    {
        SimpleRequest req;
        req.setMethod("POST");
        req.setPath("/api/v1/orders");
        if (!token.empty())
        {
            req.setHeader("Authorization", "Bearer " + token);
        }
        return req;
    }

    nlohmann::json parseJson(const std::string &body)
    {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockAuthClient> mockAuthClient_;
    std::unique_ptr<AccountIdExtractorMiddleware> middleware_;
};

// ============================================================================
// ТЕСТЫ
// ============================================================================

TEST_F(AccountIdExtractorMiddlewareTest, ValidToken_SetsAccountIdAttribute)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken("valid-token"))
        .WillOnce(Return("acc-001"));

    auto req = createRequest("valid-token");
    SimpleResponse res;

    middleware_->handle(req, res);

    // Middleware не устанавливает статус при успехе (chain продолжится)
    EXPECT_EQ(res.getStatus(), 0);
    // accountId должен быть в attributes
    EXPECT_EQ(req.getAttribute("accountId").value_or(""), "acc-001");
}

TEST_F(AccountIdExtractorMiddlewareTest, NoToken_Returns401)
{
    auto req = createRequest(); // без токена
    SimpleResponse res;

    middleware_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["error"].get<std::string>().find("Access token required") != std::string::npos);
}

TEST_F(AccountIdExtractorMiddlewareTest, InvalidToken_Returns401)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken("invalid-token"))
        .WillOnce(Return(std::nullopt));

    auto req = createRequest("invalid-token");
    SimpleResponse res;

    middleware_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["error"].get<std::string>().find("Token not valid") != std::string::npos);
}

TEST_F(AccountIdExtractorMiddlewareTest, EmptyAccountId_Returns401)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken("token-with-empty-account"))
        .WillOnce(Return(""));

    auto req = createRequest("token-with-empty-account");
    SimpleResponse res;

    middleware_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
}
