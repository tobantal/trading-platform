/**
 * @file CancelOrderHandlerTest.cpp
 * @brief Unit-тесты для CancelOrderHandler
 *
 * DELETE /api/v1/orders/{id} — отменить ордер
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/CancelOrderHandler.hpp"
#include "ports/input/IOrderService.hpp"

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

// ============================================================================
// Test Fixture
// ============================================================================

class CancelOrderHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockOrderService_ = std::make_shared<MockOrderService>();
        handler_ = std::make_unique<CancelOrderHandler>(mockOrderService_);
    }

    SimpleRequest createRequest(const std::string &method,
                                const std::string &path,
                                const std::string &accountId = "",
                                const std::string &pathPattern = "")
    {
        SimpleRequest req;
        req.setMethod(method);
        req.setPath(path);
        req.setAttribute("accountId", accountId);

        if (!pathPattern.empty())
        {
            req.setPathPattern(pathPattern);
        }

        return req;
    }

    nlohmann::json parseJson(const std::string &body)
    {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockOrderService> mockOrderService_;
    std::unique_ptr<CancelOrderHandler> handler_;
};

// ============================================================================
// ТЕСТЫ: DELETE /api/v1/orders/{id}
// ============================================================================

TEST_F(CancelOrderHandlerTest, Success_Returns200)
{
    EXPECT_CALL(*mockOrderService_, cancelOrder("acc-001", "ord-12345"))
        .WillOnce(Return(true));

    auto req = createRequest("DELETE", "/api/v1/orders/ord-12345", "acc-001", "/api/v1/orders/*");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["message"], "Order cancelled");
    EXPECT_EQ(json["order_id"], "ord-12345");
}

TEST_F(CancelOrderHandlerTest, CannotCancel_Returns400)
{
    EXPECT_CALL(*mockOrderService_, cancelOrder("acc-001", "ord-12345"))
        .WillOnce(Return(false));

    auto req = createRequest("DELETE", "/api/v1/orders/ord-12345", "acc-001", "/api/v1/orders/*");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["error"].get<std::string>().find("cancel") != std::string::npos);
}

TEST_F(CancelOrderHandlerTest, MissingOrderId_Returns400)
{
    auto req = createRequest("DELETE", "/api/v1/orders/", "acc-001");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["error"].get<std::string>().find("Order ID") != std::string::npos);
}

TEST_F(CancelOrderHandlerTest, NoAccountId_Returns500)
{
    auto req = createRequest("DELETE", "/api/v1/orders/ord-12345", "", "/api/v1/orders/*");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 500);
}

TEST_F(CancelOrderHandlerTest, GetMethod_Returns405)
{
    auto req = createRequest("GET", "/api/v1/orders/ord-12345", "acc-001");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(CancelOrderHandlerTest, PostMethod_Returns405)
{
    auto req = createRequest("POST", "/api/v1/orders/ord-12345", "acc-001");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(CancelOrderHandlerTest, ServiceThrows_Returns500)
{
    EXPECT_CALL(*mockOrderService_, cancelOrder(_, _))
        .WillOnce(Throw(std::runtime_error("Service unavailable")));

    auto req = createRequest("DELETE", "/api/v1/orders/ord-12345", "acc-001", "/api/v1/orders/*");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 500);
}
