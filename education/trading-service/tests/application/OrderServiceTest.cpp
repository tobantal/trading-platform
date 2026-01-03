/**
 * @file OrderServiceTest.cpp
 * @brief Unit tests for OrderService
 */

#include <gtest/gtest.h>
#include "application/OrderService.hpp"
#include "../mocks/MockBrokerGateway.hpp"
#include "../mocks/MockEventPublisher.hpp"
#include <nlohmann/json.hpp>

using namespace trading;
using namespace trading::application;
using namespace trading::tests;

class OrderServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockBroker_ = std::make_shared<MockBrokerGateway>();
        mockPublisher_ = std::make_shared<MockEventPublisher>();
        
        orderService_ = std::make_shared<OrderService>(mockBroker_, mockPublisher_);
    }

    std::shared_ptr<MockBrokerGateway> mockBroker_;
    std::shared_ptr<MockEventPublisher> mockPublisher_;
    std::shared_ptr<OrderService> orderService_;
};

// ============================================================================
// PLACE ORDER TESTS
// ============================================================================

TEST_F(OrderServiceTest, PlaceOrder_PublishesEvent) {
    domain::OrderRequest request;
    request.accountId = "acc-001";
    request.figi = "BBG004730N88";
    request.direction = domain::OrderDirection::BUY;
    request.type = domain::OrderType::MARKET;
    request.quantity = 10;

    auto result = orderService_->placeOrder(request);

    // Проверяем результат
    EXPECT_EQ(result.status, domain::OrderStatus::PENDING);
    EXPECT_FALSE(result.orderId.empty());
    EXPECT_EQ(result.message, "Order submitted for processing");

    // Проверяем публикацию события
    ASSERT_EQ(mockPublisher_->publishCallCount(), 1);
    
    auto& msg = mockPublisher_->getPublishedMessages()[0];
    EXPECT_EQ(msg.routingKey, "order.create");
    
    auto json = nlohmann::json::parse(msg.message);
    EXPECT_EQ(json["account_id"], "acc-001");
    EXPECT_EQ(json["figi"], "BBG004730N88");
    EXPECT_EQ(json["direction"], "BUY");
    EXPECT_EQ(json["type"], "MARKET");
    EXPECT_EQ(json["quantity"], 10);
}

TEST_F(OrderServiceTest, PlaceOrder_LimitOrder_IncludesPrice) {
    domain::OrderRequest request;
    request.accountId = "acc-001";
    request.figi = "BBG004730N88";
    request.direction = domain::OrderDirection::BUY;
    request.type = domain::OrderType::LIMIT;
    request.quantity = 10;
    request.price = domain::Money::fromDouble(275.0, "RUB");

    auto result = orderService_->placeOrder(request);

    EXPECT_EQ(result.status, domain::OrderStatus::PENDING);

    auto& msg = mockPublisher_->getPublishedMessages()[0];
    auto json = nlohmann::json::parse(msg.message);
    
    EXPECT_EQ(json["type"], "LIMIT");
    EXPECT_NEAR(json["price"].get<double>(), 275.0, 0.01);
    EXPECT_EQ(json["currency"], "RUB");
}

TEST_F(OrderServiceTest, PlaceOrder_SellOrder_CorrectDirection) {
    domain::OrderRequest request;
    request.accountId = "acc-001";
    request.figi = "BBG004730N88";
    request.direction = domain::OrderDirection::SELL;
    request.type = domain::OrderType::MARKET;
    request.quantity = 5;

    auto result = orderService_->placeOrder(request);

    auto& msg = mockPublisher_->getPublishedMessages()[0];
    auto json = nlohmann::json::parse(msg.message);
    
    EXPECT_EQ(json["direction"], "SELL");
}

TEST_F(OrderServiceTest, PlaceOrder_GeneratesUniqueIds) {
    domain::OrderRequest request;
    request.accountId = "acc-001";
    request.figi = "BBG004730N88";
    request.direction = domain::OrderDirection::BUY;
    request.type = domain::OrderType::MARKET;
    request.quantity = 10;

    auto result1 = orderService_->placeOrder(request);
    auto result2 = orderService_->placeOrder(request);
    auto result3 = orderService_->placeOrder(request);

    EXPECT_NE(result1.orderId, result2.orderId);
    EXPECT_NE(result2.orderId, result3.orderId);
    EXPECT_NE(result1.orderId, result3.orderId);
}

// ============================================================================
// CANCEL ORDER TESTS
// ============================================================================

TEST_F(OrderServiceTest, CancelOrder_PublishesEvent) {
    bool result = orderService_->cancelOrder("acc-001", "ord-001");

    EXPECT_TRUE(result);
    ASSERT_EQ(mockPublisher_->publishCallCount(), 1);

    auto& msg = mockPublisher_->getPublishedMessages()[0];
    EXPECT_EQ(msg.routingKey, "order.cancel");

    auto json = nlohmann::json::parse(msg.message);
    EXPECT_EQ(json["account_id"], "acc-001");
    EXPECT_EQ(json["order_id"], "ord-001");
}

// ============================================================================
// GET ORDERS TESTS
// ============================================================================

TEST_F(OrderServiceTest, GetAllOrders_DelegatesToBroker) {
    domain::Order order1("ord-001", "acc-001", "BBG004730N88",
                         domain::OrderDirection::BUY, domain::OrderType::MARKET,
                         10, domain::Money::fromDouble(280.0, "RUB"));
    domain::Order order2("ord-002", "acc-001", "BBG004730RP0",
                         domain::OrderDirection::SELL, domain::OrderType::LIMIT,
                         5, domain::Money::fromDouble(150.0, "RUB"));
    mockBroker_->setOrders("acc-001", {order1, order2});

    auto orders = orderService_->getAllOrders("acc-001");

    EXPECT_EQ(orders.size(), 2u);
    EXPECT_EQ(orders[0].id, "ord-001");
    EXPECT_EQ(orders[1].id, "ord-002");
}

TEST_F(OrderServiceTest, GetAllOrders_EmptyAccount_ReturnsEmpty) {
    auto orders = orderService_->getAllOrders("unknown-account");

    EXPECT_TRUE(orders.empty());
}
