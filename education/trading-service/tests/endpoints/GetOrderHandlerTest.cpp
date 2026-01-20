/**
 * @file GetOrderHandlerTest.cpp
 * @brief Unit-тесты для GetOrderHandler
 *
 * GET /api/v1/orders/{id} — ордер по ID
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/GetOrderHandler.hpp"
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

class GetOrderHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockOrderService_ = std::make_shared<MockOrderService>();
        mockAuthClient_ = std::make_shared<MockAuthClient>();
        handler_ = std::make_unique<GetOrderHandler>(mockOrderService_, mockAuthClient_);
    }

    SimpleRequest createRequest(const std::string &method,
                                const std::string &path,
                                const std::string &token = "",
                                const std::string &pathPattern = "")
    {
        SimpleRequest req;
        req.setMethod(method);
        req.setPath(path);

        if (!token.empty())
        {
            req.setHeader("Authorization", "Bearer " + token);
        }

        if (!pathPattern.empty())
        {
            req.setPathPattern(pathPattern);
        }

        return req;
    }

    domain::Order createTestOrder(const std::string &orderId,
                                  const std::string &accountId,
                                  domain::OrderStatus status = domain::OrderStatus::FILLED)
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
        order.executedPrice = domain::Money::fromDouble(270.5, "RUB");
        order.executedQuantity = 10;
        return order;
    }

    nlohmann::json parseJson(const std::string &body)
    {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockOrderService> mockOrderService_;
    std::shared_ptr<MockAuthClient> mockAuthClient_;
    std::unique_ptr<GetOrderHandler> handler_;
};

// ============================================================================
// ТЕСТЫ: GET /api/v1/orders/{id}
// ============================================================================

TEST_F(GetOrderHandlerTest, Found_Returns200)
{
    auto order = createTestOrder("ord-12345", "acc-001");

    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken("valid-token"))
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockOrderService_, getOrderById("acc-001", "ord-12345"))
        .WillOnce(Return(order));

    auto req = createRequest("GET", "/api/v1/orders/ord-12345", "valid-token", "/api/v1/orders/*");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["order_id"], "ord-12345");
    EXPECT_EQ(json["status"], "FILLED");
}

TEST_F(GetOrderHandlerTest, NotFound_Returns404)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken("valid-token"))
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockOrderService_, getOrderById("acc-001", "non-existent"))
        .WillOnce(Return(std::nullopt));

    auto req = createRequest("GET", "/api/v1/orders/non-existent", "valid-token", "/api/v1/orders/*");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 404);

    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["error"], "Order not found");
}

TEST_F(GetOrderHandlerTest, MissingOrderId_Returns400)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken("valid-token"))
        .WillOnce(Return("acc-001"));

    auto req = createRequest("GET", "/api/v1/orders/", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["error"].get<std::string>().find("Order ID") != std::string::npos);
}

TEST_F(GetOrderHandlerTest, NoToken_Returns401)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken(""))
        .WillOnce(Return(std::nullopt));

    auto req = createRequest("GET", "/api/v1/orders/ord-12345", "", "/api/v1/orders/*");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
}

TEST_F(GetOrderHandlerTest, InvalidToken_Returns401)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken("invalid-token"))
        .WillOnce(Return(std::nullopt));

    auto req = createRequest("GET", "/api/v1/orders/ord-12345", "invalid-token", "/api/v1/orders/*");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
}

TEST_F(GetOrderHandlerTest, PostMethod_Returns405)
{
    auto req = createRequest("POST", "/api/v1/orders/ord-12345", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(GetOrderHandlerTest, ServiceThrows_Returns500)
{
    EXPECT_CALL(*mockAuthClient_, getAccountIdFromToken(_))
        .WillOnce(Return("acc-001"));

    EXPECT_CALL(*mockOrderService_, getOrderById(_, _))
        .WillOnce(Throw(std::runtime_error("Service unavailable")));

    auto req = createRequest("GET", "/api/v1/orders/ord-12345", "valid-token", "/api/v1/orders/*");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 500);
}
