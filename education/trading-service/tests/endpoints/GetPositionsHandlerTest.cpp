/**
 * @file GetPositionsHandlerTest.cpp
 * @brief Unit-тесты для GetPositionsHandler
 *
 * GET /api/v1/portfolio/positions — позиции портфеля
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/GetPositionsHandler.hpp"
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

class GetPositionsHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockPortfolioService_ = std::make_shared<MockPortfolioService>();
        mockAuthClient_ = std::make_shared<MockAuthClient>();
        handler_ = std::make_unique<GetPositionsHandler>(mockPortfolioService_, mockAuthClient_);
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

    domain::Portfolio createEmptyPortfolio()
    {
        domain::Portfolio portfolio;
        portfolio.cash = domain::Money::fromDouble(100000.0, "RUB");
        portfolio.totalValue = portfolio.cash;
        return portfolio;
    }

    domain::Position createTestPosition(const std::string &figi,
                                        const std::string &ticker,
                                        int quantity)
    {
        domain::Position pos;
        pos.figi = figi;
        pos.ticker = ticker;
        pos.quantity = quantity;
        pos.averagePrice = domain::Money::fromDouble(100.0, "RUB");
        pos.currentPrice = domain::Money::fromDouble(110.0, "RUB");
        pos.pnl = domain::Money::fromDouble(1000.0, "RUB");
        pos.pnlPercent = 10.0;
        return pos;
    }

    nlohmann::json parseJson(const std::string &body)
    {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockPortfolioService> mockPortfolioService_;
    std::shared_ptr<MockAuthClient> mockAuthClient_;
    std::unique_ptr<GetPositionsHandler> handler_;
};

// ============================================================================
// ТЕСТЫ: GET /api/v1/portfolio/positions
// ============================================================================

TEST_F(GetPositionsHandlerTest, ValidToken_ReturnsPositionsArray)
{
    auto portfolio = createEmptyPortfolio();
    portfolio.positions.push_back(createTestPosition("BBG004730N88", "SBER", 100));

    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken("valid-token"))
        .Times(1)
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockPortfolioService_, getPortfolio("acc-001"))
        .Times(1)
        .WillOnce(Return(portfolio));

    auto req = createRequest("GET", "/api/v1/portfolio/positions", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 1);
}

TEST_F(GetPositionsHandlerTest, PositionFields_AllPresent)
{
    auto portfolio = createEmptyPortfolio();
    portfolio.positions.push_back(createTestPosition("BBG004730N88", "SBER", 100));

    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken(_))
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockPortfolioService_, getPortfolio(_))
        .WillOnce(Return(portfolio));

    auto req = createRequest("GET", "/api/v1/portfolio/positions", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    auto json = parseJson(res.getBody());
    auto &pos = json[0];

    EXPECT_TRUE(pos.contains("figi"));
    EXPECT_TRUE(pos.contains("ticker"));
    EXPECT_TRUE(pos.contains("quantity"));
    EXPECT_TRUE(pos.contains("average_price"));
    EXPECT_TRUE(pos.contains("current_price"));
    EXPECT_TRUE(pos.contains("currency"));
    EXPECT_TRUE(pos.contains("pnl"));
    EXPECT_TRUE(pos.contains("pnl_percent"));
}

TEST_F(GetPositionsHandlerTest, MultiplePositions_ReturnsAll)
{
    auto portfolio = createEmptyPortfolio();
    portfolio.positions.push_back(createTestPosition("BBG004730N88", "SBER", 100));
    portfolio.positions.push_back(createTestPosition("BBG006L8G4H1", "YNDX", 50));
    portfolio.positions.push_back(createTestPosition("BBG004731032", "GAZP", 200));

    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken(_))
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockPortfolioService_, getPortfolio(_))
        .WillOnce(Return(portfolio));

    auto req = createRequest("GET", "/api/v1/portfolio/positions", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    auto json = parseJson(res.getBody());
    EXPECT_EQ(json.size(), 3);
}

TEST_F(GetPositionsHandlerTest, EmptyPositions_ReturnsEmptyArray)
{
    auto portfolio = createEmptyPortfolio();

    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken(_))
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockPortfolioService_, getPortfolio(_))
        .WillOnce(Return(portfolio));

    auto req = createRequest("GET", "/api/v1/portfolio/positions", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 0);
}

TEST_F(GetPositionsHandlerTest, NoToken_Returns401)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken(""))
        .Times(1)
        .WillOnce(Return(std::nullopt));

    auto req = createRequest("GET", "/api/v1/portfolio/positions");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
}

TEST_F(GetPositionsHandlerTest, InvalidToken_Returns401)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken("invalid-token"))
        .Times(1)
        .WillOnce(Return(std::nullopt));

    auto req = createRequest("GET", "/api/v1/portfolio/positions", "invalid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
}

TEST_F(GetPositionsHandlerTest, PostMethod_Returns405)
{
    auto req = createRequest("POST", "/api/v1/portfolio/positions", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(GetPositionsHandlerTest, ServiceThrows_Returns500)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken(_))
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockPortfolioService_, getPortfolio(_))
        .WillOnce(Throw(std::runtime_error("Service unavailable")));

    auto req = createRequest("GET", "/api/v1/portfolio/positions", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 500);
}
