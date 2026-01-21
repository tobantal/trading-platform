#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/GetOrdersHandler.hpp"
#include "ports/input/IOrderService.hpp"

#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>
#include <nlohmann/json.hpp>

using namespace trading;
using namespace trading::adapters::primary;
using ::testing::_;
using ::testing::Return;

class MockOrderService : public ports::input::IOrderService
{
public:
    MOCK_METHOD(domain::OrderResult, placeOrder, (const domain::OrderRequest &), (override));
    MOCK_METHOD(bool, cancelOrder, (const std::string &, const std::string &), (override));
    MOCK_METHOD(std::optional<domain::Order>, getOrderById, (const std::string &, const std::string &), (override));
    MOCK_METHOD(std::vector<domain::Order>, getAllOrders, (const std::string &), (override));
};

class GetOrdersHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockOrderService_ = std::make_shared<MockOrderService>();
        handler_ = std::make_unique<GetOrdersHandler>(mockOrderService_);
    }

    SimpleRequest createRequest(const std::string &method,
                                const std::string &path,
                                const std::string &accountId = "")
    {
        SimpleRequest req;
        req.setMethod(method);
        req.setPath(path);
        req.setAttribute("accountId", accountId);
        return req;
    }

    nlohmann::json parseJson(const std::string &body)
    {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockOrderService> mockOrderService_;
    std::unique_ptr<GetOrdersHandler> handler_;
};

TEST_F(GetOrdersHandlerTest, ReturnsOrders_Returns200)
{
    std::vector<domain::Order> orders;
    domain::Order order;
    order.id = "ord-001";
    order.accountId = "acc-001";
    order.status = domain::OrderStatus::FILLED;
    orders.push_back(order);

    EXPECT_CALL(*mockOrderService_, getAllOrders("acc-001"))
        .WillOnce(Return(orders));

    auto req = createRequest("GET", "/api/v1/orders", "acc-001");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["orders"].size(), 1);
}

TEST_F(GetOrdersHandlerTest, NoAccountId_Returns500)
{
    auto req = createRequest("GET", "/api/v1/orders", "");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 500);
}

TEST_F(GetOrdersHandlerTest, PostMethod_Returns405)
{
    auto req = createRequest("POST", "/api/v1/orders", "acc-001");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 405);
}
