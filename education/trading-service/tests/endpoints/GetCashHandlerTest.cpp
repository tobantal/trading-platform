/**
 * @file GetCashHandlerTest.cpp
 * @brief Unit-тесты для GetCashHandler
 *
 * GET /api/v1/portfolio/cash — доступные средства
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/GetCashHandler.hpp"
#include "ports/input/IPortfolioService.hpp"
#include "ports/output/IAuthClient.hpp"

#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>
#include <nlohmann/json.hpp>

using namespace trading;
using namespace trading::adapters::primary;
using ::testing::_;
using ::testing::Return;
using ::testing::Throw;

// ============================================================================
// Mocks
// ============================================================================
class MockPortfolioService : public ports::input::IPortfolioService {
public:
    MOCK_METHOD(domain::Portfolio, getPortfolio, (const std::string&), (override));
    MOCK_METHOD(domain::Money, getAvailableCash, (const std::string&), (override));
    MOCK_METHOD(std::vector<domain::Position>, getPositions, (const std::string&), (override));
};

class MockAuthClient : public ports::output::IAuthClient
{
public:
    MOCK_METHOD(ports::output::TokenValidationResult, validateAccessToken, (const std::string &), (override));
    MOCK_METHOD(std::optional<std::string>, getAccountIdFromToken, (const std::string &), (override));
};

// ============================================================================
// Test Fixture
// ============================================================================

class GetCashHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockPortfolioService_ = std::make_shared<MockPortfolioService>();
        mockAuthClient_ = std::make_shared<MockAuthClient>();
        handler_ = std::make_unique<GetCashHandler>(mockPortfolioService_, mockAuthClient_);
    }

    SimpleRequest createRequest(const std::string &method,
                                const std::string &path,
                                const std::string &token = "")
    {
        SimpleRequest req;
        req.setMethod(method);
        req.setPath(path);

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

    std::shared_ptr<MockPortfolioService> mockPortfolioService_;
    std::shared_ptr<MockAuthClient> mockAuthClient_;
    std::unique_ptr<GetCashHandler> handler_;
};

// ============================================================================
// ТЕСТЫ: GET /api/v1/portfolio/cash
// ============================================================================

TEST_F(GetCashHandlerTest, ValidToken_ReturnsCash)
{
    auto cash = domain::Money::fromDouble(100000.0, "RUB");

    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken("valid-token"))
        .Times(1)
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockPortfolioService_, getAvailableCash("acc-001"))
        .Times(1)
        .WillOnce(Return(cash));

    auto req = createRequest("GET", "/api/v1/portfolio/cash", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["amount"], 100000.0);
    EXPECT_EQ(json["currency"], "RUB");
}

TEST_F(GetCashHandlerTest, CashFields_AllPresent)
{
    auto cash = domain::Money::fromDouble(50000.0, "RUB");

    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken(_))
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockPortfolioService_, getAvailableCash(_))
        .WillOnce(Return(cash));

    auto req = createRequest("GET", "/api/v1/portfolio/cash", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("amount"));
    EXPECT_TRUE(json.contains("currency"));
}

TEST_F(GetCashHandlerTest, DifferentCurrency_ReturnsCorrectCurrency)
{
    auto cash = domain::Money::fromDouble(5000.0, "USD");

    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken(_))
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockPortfolioService_, getAvailableCash(_))
        .WillOnce(Return(cash));

    auto req = createRequest("GET", "/api/v1/portfolio/cash", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["amount"], 5000.0);
    EXPECT_EQ(json["currency"], "USD");
}

TEST_F(GetCashHandlerTest, ZeroCash_ReturnsZero)
{
    auto cash = domain::Money::fromDouble(0.0, "RUB");

    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken(_))
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockPortfolioService_, getAvailableCash(_))
        .WillOnce(Return(cash));

    auto req = createRequest("GET", "/api/v1/portfolio/cash", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["amount"], 0.0);
}

TEST_F(GetCashHandlerTest, NoToken_Returns401)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken(""))
        .Times(1)
        .WillOnce(Return(std::nullopt));

    auto req = createRequest("GET", "/api/v1/portfolio/cash");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("error"));
}

TEST_F(GetCashHandlerTest, InvalidToken_Returns401)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken("invalid-token"))
        .Times(1)
        .WillOnce(Return(std::nullopt));

    auto req = createRequest("GET", "/api/v1/portfolio/cash", "invalid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
}

TEST_F(GetCashHandlerTest, PostMethod_Returns405)
{
    auto req = createRequest("POST", "/api/v1/portfolio/cash", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(GetCashHandlerTest, PutMethod_Returns405)
{
    auto req = createRequest("PUT", "/api/v1/portfolio/cash", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(GetCashHandlerTest, ServiceThrows_Returns500)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken(_))
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockPortfolioService_, getAvailableCash(_))
        .WillOnce(Throw(std::runtime_error("Service unavailable")));

    auto req = createRequest("GET", "/api/v1/portfolio/cash", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 500);
}
