/**
 * @file CreateOrderHandlerTest.cpp
 * @brief Unit-тесты для CreateOrderHandler
 *
 * POST /api/v1/orders — создать ордер
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/CreateOrderHandler.hpp"
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

class CreateOrderHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockOrderService_ = std::make_shared<MockOrderService>();
        handler_ = std::make_unique<CreateOrderHandler>(mockOrderService_);
    }

    SimpleRequest createRequest(const std::string &method,
                                const std::string &path,
                                const std::string &body = "",
                                const std::string &accountId = ""
                            )
    {
        SimpleRequest req;

        req.setAttribute("accountId", accountId); // сичтаем, что уже пришел распрарсенный accountId согласно pipeline

        req.setMethod(method);
        req.setPath(path);
        req.setBody(body);

        return req;
    }

    domain::OrderResult createSuccessResult(const std::string &orderId)
    {
        domain::OrderResult result;
        result.orderId = orderId;
        result.status = domain::OrderStatus::PENDING;
        result.message = "Order placed";
        return result;
    }

    domain::OrderResult createRejectedResult()
    {
        domain::OrderResult result;
        result.orderId = "";
        result.status = domain::OrderStatus::REJECTED;
        result.message = "Insufficient funds";
        return result;
    }

    nlohmann::json parseJson(const std::string &body)
    {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockOrderService> mockOrderService_;
    std::unique_ptr<CreateOrderHandler> handler_;
};

// ============================================================================
// ТЕСТЫ: POST /api/v1/orders
// ============================================================================

TEST_F(CreateOrderHandlerTest, ValidMarketOrder_Returns0)
{
    EXPECT_CALL(*mockOrderService_, placeOrder(_))
        .WillOnce(Return(createSuccessResult("ord-12345")));

    std::string body = R"({
        "figi": "BBG004730N88",
        "quantity": 10,
        "direction": "BUY",
        "type": "MARKET"
    })";

    auto req = createRequest("POST", "/api/v1/orders", body, "acc-001");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 0);
    EXPECT_EQ(req.getAttribute("httpStatus"), "201");

    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["order_id"], "ord-12345");
    EXPECT_EQ(json["status"], "PENDING");
}

TEST_F(CreateOrderHandlerTest, ValidLimitOrder_Returns0)
{
    EXPECT_CALL(*mockOrderService_, placeOrder(_))
        .WillOnce(Return(createSuccessResult("ord-12345")));

    std::string body = R"({
        "figi": "BBG004730N88",
        "quantity": 10,
        "direction": "SELL",
        "type": "LIMIT",
        "price": 270.50,
        "currency": "RUB"
    })";

    auto req = createRequest("POST", "/api/v1/orders", body, "acc-001");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 0);
    EXPECT_EQ(req.getAttribute("httpStatus"), "201");
}

TEST_F(CreateOrderHandlerTest, NoAccountId_Returns500)
{
    std::string body = R"({
        "figi": "BBG004730N88",
        "quantity": 10,
        "direction": "SELL",
        "type": "LIMIT",
        "price": 270.50,
        "currency": "RUB"
    })";

    auto req = createRequest("POST", "/api/v1/orders", body, ""); // нет accounId, ошибка pipeline
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 500);
}

TEST_F(CreateOrderHandlerTest, RejectedOrder_Returns400)
{
    EXPECT_CALL(*mockOrderService_, placeOrder(_))
        .WillOnce(Return(createRejectedResult()));

    std::string body = R"({
        "figi": "BBG004730N88",
        "quantity": 10,
        "direction": "BUY",
        "type": "MARKET"
    })";

    auto req = createRequest("POST", "/api/v1/orders", body, "acc-001");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);

    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["status"], "REJECTED");
}

TEST_F(CreateOrderHandlerTest, MissingFigi_Returns400)
{
    std::string body = R"({
        "quantity": 10,
        "direction": "BUY",
        "type": "MARKET"
    })";

    auto req = createRequest("POST", "/api/v1/orders", body, "acc-001");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["error"].get<std::string>().find("FIGI") != std::string::npos);
}

TEST_F(CreateOrderHandlerTest, ZeroQuantity_Returns400)
{
    std::string body = R"({
        "figi": "BBG004730N88",
        "quantity": 0,
        "direction": "BUY",
        "type": "MARKET"
    })";

    auto req = createRequest("POST", "/api/v1/orders", body, "acc-001");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["error"].get<std::string>().find("Quantity") != std::string::npos);
}

TEST_F(CreateOrderHandlerTest, InvalidJson_Returns400)
{
    auto req = createRequest("POST", "/api/v1/orders", "not json", "acc-001");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["error"].get<std::string>().find("JSON") != std::string::npos);
}

TEST_F(CreateOrderHandlerTest, GetMethod_Returns405)
{
    auto req = createRequest("GET", "/api/v1/orders", "", "acc-001");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 405);
}
