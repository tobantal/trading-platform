/**
 * @file GetOrdersHandlerTest.cpp
 * @brief Unit-тесты для GetOrdersHandler
 *
 * GET /api/v1/orders — список ордеров
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/GetOrdersHandler.hpp"
#include "ports/input/IOrderService.hpp"
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

class MockOrderService : public ports::input::IOrderService
{
public:
    MOCK_METHOD(domain::OrderResult, placeOrder, (const domain::OrderRequest &), (override));
    MOCK_METHOD(bool, cancelOrder, (const std::string &, const std::string &), (override));
    MOCK_METHOD(std::optional<domain::Order>, getOrderById, (const std::string &, const std::string &), (override));
    MOCK_METHOD(std::vector<domain::Order>, getAllOrders, (const std::string &), (override));
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

class GetOrdersHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockOrderService_ = std::make_shared<MockOrderService>();
        mockAuthClient_ = std::make_shared<MockAuthClient>();
        handler_ = std::make_unique<GetOrdersHandler>(mockOrderService_, mockAuthClient_);
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

    domain::Order createTestOrder(const std::string &orderId,
                                  const std::string &accountId,
                                  domain::OrderStatus status = domain::OrderStatus::PENDING)
    {
        domain::Order order;
        order.id = orderId;
        order.accountId = accountId;
        order.figi = "BBG004730N88";
        order.direction = domain::OrderDirection::BUY;
        order.type = domain::OrderType::MARKET;
        order.quantity = 10;
        order.status = status;
        order.price = domain::Money::fromDouble(270.0, "RUB");
        return order;
    }

    nlohmann::json parseJson(const std::string &body)
    {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockOrderService> mockOrderService_;
    std::shared_ptr<MockAuthClient> mockAuthClient_;
    std::unique_ptr<GetOrdersHandler> handler_;
};

// ============================================================================
// ТЕСТЫ: GET /api/v1/orders
// ============================================================================

TEST_F(GetOrdersHandlerTest, ValidToken_ReturnsOrdersArray)
{
    std::vector<domain::Order> orders = {
        createTestOrder("ord-001", "acc-001"),
        createTestOrder("ord-002", "acc-001", domain::OrderStatus::FILLED)};

    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken("valid-token"))
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockOrderService_, getAllOrders("acc-001"))
        .WillOnce(Return(orders));

    auto req = createRequest("GET", "/api/v1/orders", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("orders"));
    EXPECT_EQ(json["orders"].size(), 2);
}

TEST_F(GetOrdersHandlerTest, OrderFields_AllPresent)
{
    std::vector<domain::Order> orders = {
        createTestOrder("ord-001", "acc-001")};

    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken(_))
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockOrderService_, getAllOrders(_))
        .WillOnce(Return(orders));

    auto req = createRequest("GET", "/api/v1/orders", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    auto json = parseJson(res.getBody());
    auto &order = json["orders"][0];

    EXPECT_TRUE(order.contains("order_id"));
    EXPECT_TRUE(order.contains("account_id"));
    EXPECT_TRUE(order.contains("figi"));
    EXPECT_TRUE(order.contains("direction"));
    EXPECT_TRUE(order.contains("type"));
    EXPECT_TRUE(order.contains("quantity"));
    EXPECT_TRUE(order.contains("status"));
}

TEST_F(GetOrdersHandlerTest, EmptyOrders_ReturnsEmptyArray)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken("valid-token"))
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockOrderService_, getAllOrders("acc-001"))
        .WillOnce(Return(std::vector<domain::Order>{}));

    auto req = createRequest("GET", "/api/v1/orders", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["orders"].is_array());
    EXPECT_EQ(json["orders"].size(), 0);
}

TEST_F(GetOrdersHandlerTest, NoToken_Returns401)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken(""))
        .WillOnce(Return(std::nullopt));

    auto req = createRequest("GET", "/api/v1/orders");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
}

TEST_F(GetOrdersHandlerTest, InvalidToken_Returns401)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken("invalid-token"))
        .WillOnce(Return(std::nullopt));

    auto req = createRequest("GET", "/api/v1/orders", "invalid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
}

TEST_F(GetOrdersHandlerTest, PostMethod_Returns405)
{
    auto req = createRequest("POST", "/api/v1/orders", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(GetOrdersHandlerTest, ServiceThrows_Returns500)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken(_))
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockOrderService_, getAllOrders(_))
        .WillOnce(Throw(std::runtime_error("Service unavailable")));

    auto req = createRequest("GET", "/api/v1/orders", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 500);
}
