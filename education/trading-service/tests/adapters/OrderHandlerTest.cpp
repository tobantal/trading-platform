/**
 * @file OrderHandlerTest.cpp
 * @brief Unit tests for OrderHandler
 */

#include <gtest/gtest.h>
#include "adapters/primary/OrderHandler.hpp"
#include "application/OrderService.hpp"
#include "../mocks/MockBrokerGateway.hpp"
#include "../mocks/MockEventPublisher.hpp"
#include "../mocks/MockAuthClient.hpp"
#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>
#include <nlohmann/json.hpp>

using namespace trading;
using namespace trading::adapters::primary;
using namespace trading::application;
using namespace trading::tests;
using json = nlohmann::json;

class OrderHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockBroker_ = std::make_shared<MockBrokerGateway>();
        mockPublisher_ = std::make_shared<MockEventPublisher>();
        mockAuthClient_ = std::make_shared<MockAuthClient>();

        auto orderService = std::make_shared<OrderService>(mockBroker_, mockPublisher_);
        handler_ = std::make_shared<OrderHandler>(orderService, mockAuthClient_);

        // Настраиваем валидный токен
        mockAuthClient_->addValidToken("valid-token", "user-001", "acc-001");
    }

    SimpleRequest createRequest(
        const std::string& method,
        const std::string& path,
        const std::string& body = "",
        const std::string& token = ""
    ) {
        std::map<std::string, std::string> headers;
        headers["Content-Type"] = "application/json";
        if (!token.empty()) {
            headers["Authorization"] = "Bearer " + token;
        }
        return SimpleRequest(method, path, body, "127.0.0.1", 8080, headers);
    }

    std::shared_ptr<MockBrokerGateway> mockBroker_;
    std::shared_ptr<MockEventPublisher> mockPublisher_;
    std::shared_ptr<MockAuthClient> mockAuthClient_;
    std::shared_ptr<OrderHandler> handler_;
};

// ============================================================================
// AUTHORIZATION TESTS
// ============================================================================

TEST_F(OrderHandlerTest, NoToken_Returns401) {
    auto req = createRequest("POST", "/api/v1/orders", R"({"figi":"BBG004730N88","quantity":10})");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
    auto body = json::parse(res.getBody());
    EXPECT_TRUE(body.contains("error"));
}

TEST_F(OrderHandlerTest, InvalidToken_Returns401) {
    auto req = createRequest("POST", "/api/v1/orders", 
                             R"({"figi":"BBG004730N88","quantity":10})", 
                             "invalid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 401);
}

// ============================================================================
// CREATE ORDER TESTS
// ============================================================================

TEST_F(OrderHandlerTest, CreateOrder_ValidRequest_Returns201) {
    auto req = createRequest("POST", "/api/v1/orders",
        R"({"figi":"BBG004730N88","quantity":10,"direction":"BUY","type":"MARKET"})",
        "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 201);
    auto body = json::parse(res.getBody());
    EXPECT_TRUE(body.contains("order_id"));
    EXPECT_EQ(body["status"], "PENDING");
}

TEST_F(OrderHandlerTest, CreateOrder_MissingFigi_Returns400) {
    auto req = createRequest("POST", "/api/v1/orders",
        R"({"quantity":10,"direction":"BUY","type":"MARKET"})",
        "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);
    auto body = json::parse(res.getBody());
    EXPECT_EQ(body["error"], "FIGI is required");
}

TEST_F(OrderHandlerTest, CreateOrder_ZeroQuantity_Returns400) {
    auto req = createRequest("POST", "/api/v1/orders",
        R"({"figi":"BBG004730N88","quantity":0,"direction":"BUY","type":"MARKET"})",
        "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);
    auto body = json::parse(res.getBody());
    EXPECT_EQ(body["error"], "Quantity must be positive");
}

TEST_F(OrderHandlerTest, CreateOrder_LimitOrder_IncludesPrice) {
    auto req = createRequest("POST", "/api/v1/orders",
        R"({"figi":"BBG004730N88","quantity":10,"direction":"BUY","type":"LIMIT","price":275.0})",
        "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 201);

    // Проверяем что событие содержит цену
    ASSERT_GE(mockPublisher_->publishCallCount(), 1);
    auto msg = json::parse(mockPublisher_->getPublishedMessages()[0].message);
    EXPECT_EQ(msg["type"], "LIMIT");
    EXPECT_NEAR(msg["price"].get<double>(), 275.0, 0.01);
}

TEST_F(OrderHandlerTest, CreateOrder_InvalidJson_Returns400) {
    auto req = createRequest("POST", "/api/v1/orders",
        "not a json",
        "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);
    auto body = json::parse(res.getBody());
    EXPECT_EQ(body["error"], "Invalid JSON");
}

// ============================================================================
// GET ORDERS TESTS
// ============================================================================

TEST_F(OrderHandlerTest, GetOrders_ValidToken_ReturnsOrders) {
    domain::Order order("ord-001", "acc-001", "BBG004730N88",
                        domain::OrderDirection::BUY, domain::OrderType::MARKET,
                        10, domain::Money::fromDouble(280.0, "RUB"));
    mockBroker_->setOrders("acc-001", {order});

    auto req = createRequest("GET", "/api/v1/orders", "", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);
    auto body = json::parse(res.getBody());
    EXPECT_TRUE(body.contains("orders"));
    EXPECT_EQ(body["orders"].size(), 1u);
    EXPECT_EQ(body["orders"][0]["id"], "ord-001");
}

TEST_F(OrderHandlerTest, GetOrders_NoOrders_ReturnsEmptyArray) {
    auto req = createRequest("GET", "/api/v1/orders", "", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);
    auto body = json::parse(res.getBody());
    EXPECT_TRUE(body["orders"].empty());
}

// ============================================================================
// CANCEL ORDER TESTS
// ============================================================================

TEST_F(OrderHandlerTest, CancelOrder_ValidRequest_Returns200) {
    auto req = createRequest("DELETE", "/api/v1/orders/ord-001", "", "valid-token");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);
    auto body = json::parse(res.getBody());
    EXPECT_EQ(body["order_id"], "ord-001");

    // Проверяем публикацию события
    ASSERT_GE(mockPublisher_->publishCallCount(), 1);
    EXPECT_EQ(mockPublisher_->getPublishedMessages().back().routingKey, "order.cancel");
}
