#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/GetOrderHandler.hpp"
#include "ports/input/IOrderService.hpp"

#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>
#include <nlohmann/json.hpp>

using namespace trading;
using namespace trading::adapters::primary;
using ::testing::_;
using ::testing::Return;
using ::testing::Throw;

class MockOrderService : public ports::input::IOrderService
{
public:
    MOCK_METHOD(domain::OrderResult, placeOrder, (const domain::OrderRequest &), (override));
    MOCK_METHOD(bool, cancelOrder, (const std::string &, const std::string &), (override));
    MOCK_METHOD(std::optional<domain::Order>, getOrderById, (const std::string &, const std::string &), (override));
    MOCK_METHOD(std::vector<domain::Order>, getAllOrders, (const std::string &), (override));
};

class GetOrderHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockOrderService_ = std::make_shared<MockOrderService>();
        handler_ = std::make_unique<GetOrderHandler>(mockOrderService_);
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
    std::unique_ptr<GetOrderHandler> handler_;
};

TEST_F(GetOrderHandlerTest, Found_Returns200)
{
    auto order = createTestOrder("ord-12345", "acc-001");

    EXPECT_CALL(*mockOrderService_, getOrderById("acc-001", "ord-12345"))
        .WillOnce(Return(order));

    auto req = createRequest("GET", "/api/v1/orders/ord-12345", "acc-001", "/api/v1/orders/*");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["order_id"], "ord-12345");
    EXPECT_EQ(json["status"], "FILLED");
}

TEST_F(GetOrderHandlerTest, NotFound_Returns404)
{
    EXPECT_CALL(*mockOrderService_, getOrderById("acc-001", "ord-99999"))
        .WillOnce(Return(std::nullopt));

    auto req = createRequest("GET", "/api/v1/orders/ord-99999", "acc-001", "/api/v1/orders/*");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 404);
}

TEST_F(GetOrderHandlerTest, NoAccountId_Returns500)
{
    auto req = createRequest("GET", "/api/v1/orders/ord-12345", "", "/api/v1/orders/*");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 500);
}

TEST_F(GetOrderHandlerTest, PostMethod_Returns405)
{
    auto req = createRequest("POST", "/api/v1/orders/ord-12345", "acc-001");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 405);
}
