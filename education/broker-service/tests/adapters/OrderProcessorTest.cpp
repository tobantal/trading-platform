/**
 * @file OrderProcessorTest.cpp
 * @brief Unit tests for OrderProcessor
 */

#include <gtest/gtest.h>
#include "adapters/secondary/broker/OrderProcessor.hpp"

using namespace broker::adapters::secondary;

class OrderProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        priceSimulator_ = std::make_shared<PriceSimulator>();
        priceSimulator_->initInstrument("SBER", 280.0, 0.02);
        priceSimulator_->initInstrument("GAZP", 150.0, 0.025);
        
        processor_ = std::make_unique<OrderProcessor>(priceSimulator_);
    }

    void TearDown() override {
        processor_.reset();
        priceSimulator_.reset();
    }

    OrderRequest createBuyMarket(const std::string& figi, int64_t qty) {
        OrderRequest req;
        req.accountId = "test-account";
        req.figi = figi;
        req.direction = Direction::BUY;
        req.type = Type::MARKET;
        req.quantity = qty;
        return req;
    }

    OrderRequest createSellMarket(const std::string& figi, int64_t qty) {
        OrderRequest req;
        req.accountId = "test-account";
        req.figi = figi;
        req.direction = Direction::SELL;
        req.type = Type::MARKET;
        req.quantity = qty;
        return req;
    }

    OrderRequest createBuyLimit(const std::string& figi, int64_t qty, double price) {
        OrderRequest req;
        req.accountId = "test-account";
        req.figi = figi;
        req.direction = Direction::BUY;
        req.type = Type::LIMIT;
        req.quantity = qty;
        req.price = price;
        return req;
    }

    OrderRequest createSellLimit(const std::string& figi, int64_t qty, double price) {
        OrderRequest req;
        req.accountId = "test-account";
        req.figi = figi;
        req.direction = Direction::SELL;
        req.type = Type::LIMIT;
        req.quantity = qty;
        req.price = price;
        return req;
    }

    std::shared_ptr<PriceSimulator> priceSimulator_;
    std::unique_ptr<OrderProcessor> processor_;
};

// ============================================================================
// IMMEDIATE MODE TESTS
// ============================================================================

TEST_F(OrderProcessorTest, Immediate_MarketBuy_Fills) {
    auto scenario = MarketScenario::immediate(280.0);
    auto req = createBuyMarket("SBER", 10);
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
    EXPECT_EQ(result.executedQuantity, 10);
    EXPECT_GT(result.executedPrice, 0.0);
}

TEST_F(OrderProcessorTest, Immediate_MarketSell_Fills) {
    auto scenario = MarketScenario::immediate(280.0);
    auto req = createSellMarket("SBER", 5);
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
    EXPECT_EQ(result.executedQuantity, 5);
}

TEST_F(OrderProcessorTest, Immediate_LimitBuy_Fills) {
    auto scenario = MarketScenario::immediate(280.0);
    auto req = createBuyLimit("SBER", 10, 290.0);
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
}

TEST_F(OrderProcessorTest, Immediate_UnknownFigi_Rejects) {
    auto scenario = MarketScenario::immediate(280.0);
    auto req = createBuyMarket("UNKNOWN", 10);
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::REJECTED);
    EXPECT_FALSE(result.message.empty());
}

// ============================================================================
// REALISTIC MODE TESTS
// ============================================================================

TEST_F(OrderProcessorTest, Realistic_MarketOrder_Fills) {
    auto scenario = MarketScenario::realistic(280.0);
    auto req = createBuyMarket("SBER", 10);
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
    EXPECT_GT(result.executedPrice, 0.0);
}

TEST_F(OrderProcessorTest, Realistic_MarketOrder_WithSlippage) {
    auto scenario = MarketScenario::lowLiquidity(280.0, 100);
    auto req = createBuyMarket("SBER", 50);  // >10% of liquidity
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
    // Price should be higher due to slippage
}

TEST_F(OrderProcessorTest, Realistic_MarketSell_WithSlippage) {
    auto scenario = MarketScenario::lowLiquidity(280.0, 100);
    auto req = createSellMarket("SBER", 50);
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
}

TEST_F(OrderProcessorTest, Realistic_SmallOrder_NoSlippage) {
    auto scenario = MarketScenario::realistic(280.0);
    scenario.availableLiquidity = 10000;
    auto req = createBuyMarket("SBER", 5);  // <10% of liquidity
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
}

TEST_F(OrderProcessorTest, Realistic_LimitBuy_BelowAsk_Queued) {
    auto scenario = MarketScenario::realistic(280.0);
    auto req = createBuyLimit("SBER", 10, 270.0);  // Below current price
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::PENDING);
    EXPECT_EQ(processor_->pendingCount(), 1u);
}

TEST_F(OrderProcessorTest, Realistic_LimitBuy_AboveAsk_Fills) {
    auto scenario = MarketScenario::realistic(280.0);
    auto req = createBuyLimit("SBER", 10, 300.0);  // Above current price
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
}

TEST_F(OrderProcessorTest, Realistic_LimitSell_AboveBid_Queued) {
    auto scenario = MarketScenario::realistic(280.0);
    auto req = createSellLimit("SBER", 10, 290.0);  // Above current price
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::PENDING);
}

TEST_F(OrderProcessorTest, Realistic_LimitSell_BelowBid_Fills) {
    auto scenario = MarketScenario::realistic(280.0);
    auto req = createSellLimit("SBER", 10, 260.0);  // Below current price
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
}

// ============================================================================
// PARTIAL FILL TESTS
// ============================================================================

TEST_F(OrderProcessorTest, Partial_FillsPartially) {
    auto scenario = MarketScenario::partialFill(280.0, 0.5);  // 50%
    auto req = createBuyMarket("SBER", 10);
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::PARTIALLY_FILLED);
    EXPECT_EQ(result.executedQuantity, 5);
}

TEST_F(OrderProcessorTest, Partial_MinimumOneLot) {
    auto scenario = MarketScenario::partialFill(280.0, 0.01);  // 1%
    auto req = createBuyMarket("SBER", 10);
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_GE(result.executedQuantity, 1);
}

TEST_F(OrderProcessorTest, Partial_FullFillAtRatio1) {
    auto scenario = MarketScenario::partialFill(280.0, 1.0);  // 100%
    auto req = createBuyMarket("SBER", 10);
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
    EXPECT_EQ(result.executedQuantity, 10);
}

// ============================================================================
// ALWAYS REJECT TESTS
// ============================================================================

TEST_F(OrderProcessorTest, AlwaysReject_MarketOrder) {
    auto scenario = MarketScenario::alwaysReject("Test rejection");
    auto req = createBuyMarket("SBER", 10);
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::REJECTED);
    EXPECT_EQ(result.message, "Test rejection");
}

TEST_F(OrderProcessorTest, AlwaysReject_DefaultReason) {
    auto scenario = MarketScenario::alwaysReject();
    auto req = createBuyMarket("SBER", 10);
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::REJECTED);
}

TEST_F(OrderProcessorTest, RejectProbability_100Percent) {
    auto scenario = MarketScenario::realistic(280.0);
    scenario.rejectProbability = 1.0;
    scenario.rejectReason = "Always reject";
    auto req = createBuyMarket("SBER", 10);
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::REJECTED);
}

TEST_F(OrderProcessorTest, RejectProbability_0Percent) {
    auto scenario = MarketScenario::realistic(280.0);
    scenario.rejectProbability = 0.0;
    auto req = createBuyMarket("SBER", 10);
    
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_NE(result.status, Status::REJECTED);
}

// ============================================================================
// PENDING ORDER MANAGEMENT
// ============================================================================

TEST_F(OrderProcessorTest, CancelOrder_Success) {
    auto scenario = MarketScenario::realistic(280.0);
    auto req = createBuyLimit("SBER", 10, 270.0);  // Will be pending
    
    auto result = processor_->processOrder(req, scenario);
    EXPECT_EQ(result.status, Status::PENDING);
    
    bool cancelled = processor_->cancelOrder(result.orderId);
    EXPECT_TRUE(cancelled);
    EXPECT_EQ(processor_->pendingCount(), 0u);
}

TEST_F(OrderProcessorTest, CancelOrder_NotFound) {
    bool cancelled = processor_->cancelOrder("non-existent-order");
    EXPECT_FALSE(cancelled);
}

TEST_F(OrderProcessorTest, GetPendingOrders_ReturnsAll) {
    auto scenario = MarketScenario::realistic(280.0);
    
    processor_->processOrder(createBuyLimit("SBER", 10, 270.0), scenario);
    processor_->processOrder(createBuyLimit("GAZP", 5, 140.0), scenario);
    
    auto pending = processor_->getPendingOrders();
    EXPECT_EQ(pending.size(), 2u);
}

// ============================================================================
// DELAYED MODE TESTS
// ============================================================================

TEST_F(OrderProcessorTest, Delayed_GoesToPending) {
    auto scenario = MarketScenario::realistic(280.0);
    scenario.fillBehavior = OrderFillBehavior::DELAYED;
    scenario.fillDelay = std::chrono::milliseconds{100};
    
    auto req = createBuyMarket("SBER", 10);
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::PENDING);
    EXPECT_EQ(processor_->pendingCount(), 1u);
}

// ============================================================================
// FILL CALLBACK TESTS
// ============================================================================

TEST_F(OrderProcessorTest, FillCallback_CalledOnPendingFill) {
    bool callbackCalled = false;
    OrderFillEvent receivedEvent;
    
    processor_->setFillCallback([&](const OrderFillEvent& event) {
        callbackCalled = true;
        receivedEvent = event;
    });
    
    auto scenario = MarketScenario::realistic(280.0);
    auto req = createBuyLimit("SBER", 10, 270.0);
    auto result = processor_->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::PENDING);
    
    // Simulate price drop to trigger fill
    priceSimulator_->initInstrument("SBER", 265.0, 0.02);
    processor_->processPendingOrders(scenario);
    
    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(receivedEvent.orderId, result.orderId);
    EXPECT_EQ(receivedEvent.quantity, 10);
}
