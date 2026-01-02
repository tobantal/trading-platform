/**
 * @file MarketScenarioTest.cpp
 * @brief Unit tests for MarketScenario
 */

#include <gtest/gtest.h>
#include "adapters/secondary/broker/MarketScenario.hpp"

using namespace broker::adapters::secondary;

class MarketScenarioTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// DEFAULT VALUES
// ============================================================================

TEST_F(MarketScenarioTest, DefaultValues) {
    MarketScenario scenario;
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 100.0);
    EXPECT_DOUBLE_EQ(scenario.volatility, 0.01);
    EXPECT_DOUBLE_EQ(scenario.bidAskSpread, 0.1);
    EXPECT_DOUBLE_EQ(scenario.partialFillRatio, 1.0);
    EXPECT_EQ(scenario.availableLiquidity, 10000);
    EXPECT_DOUBLE_EQ(scenario.slippagePercent, 0.001);
    EXPECT_EQ(scenario.executionDelay.count(), 0);
    EXPECT_DOUBLE_EQ(scenario.rejectProbability, 0.0);
    EXPECT_TRUE(scenario.rejectReason.empty());
}

// ============================================================================
// FACTORY METHODS
// ============================================================================

TEST_F(MarketScenarioTest, FactoryImmediate) {
    auto scenario = MarketScenario::immediate(280.0);
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 280.0);
    EXPECT_EQ(scenario.fillBehavior, OrderFillBehavior::IMMEDIATE);
    EXPECT_DOUBLE_EQ(scenario.volatility, 0.0);
    EXPECT_DOUBLE_EQ(scenario.slippage, 0.0);
}

TEST_F(MarketScenarioTest, FactoryRealistic) {
    auto scenario = MarketScenario::realistic(280.0, 0.002, 0.005);
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 280.0);
    EXPECT_EQ(scenario.fillBehavior, OrderFillBehavior::REALISTIC);
    EXPECT_DOUBLE_EQ(scenario.volatility, 0.002);
    EXPECT_DOUBLE_EQ(scenario.slippagePercent, 0.005);
}

TEST_F(MarketScenarioTest, FactoryRealistic_DefaultValues) {
    auto scenario = MarketScenario::realistic(280.0);
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 280.0);
    EXPECT_EQ(scenario.fillBehavior, OrderFillBehavior::REALISTIC);
    EXPECT_DOUBLE_EQ(scenario.volatility, 0.02);
    EXPECT_DOUBLE_EQ(scenario.slippagePercent, 0.001);
}

TEST_F(MarketScenarioTest, FactoryLowLiquidity) {
    auto scenario = MarketScenario::lowLiquidity(280.0, 50);
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 280.0);
    EXPECT_EQ(scenario.availableLiquidity, 50);
    EXPECT_GT(scenario.slippagePercent, 0.005);  // Высокое проскальзывание
}

TEST_F(MarketScenarioTest, FactoryLowLiquidity_DefaultLiquidity) {
    auto scenario = MarketScenario::lowLiquidity(280.0);
    
    EXPECT_EQ(scenario.availableLiquidity, 100);  // По умолчанию 100
}

TEST_F(MarketScenarioTest, FactoryPartialFill) {
    auto scenario = MarketScenario::partialFill(280.0, 0.3);
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 280.0);
    EXPECT_EQ(scenario.fillBehavior, OrderFillBehavior::PARTIAL);
    EXPECT_DOUBLE_EQ(scenario.partialFillRatio, 0.3);
}

TEST_F(MarketScenarioTest, FactoryPartialFill_DefaultRatio) {
    auto scenario = MarketScenario::partialFill(280.0);
    
    EXPECT_DOUBLE_EQ(scenario.partialFillRatio, 0.5);  // По умолчанию 50%
}

TEST_F(MarketScenarioTest, FactoryAlwaysReject) {
    auto scenario = MarketScenario::alwaysReject("Insufficient funds");
    
    EXPECT_EQ(scenario.fillBehavior, OrderFillBehavior::ALWAYS_REJECT);
    EXPECT_DOUBLE_EQ(scenario.rejectProbability, 1.0);
    EXPECT_EQ(scenario.rejectReason, "Insufficient funds");
}

TEST_F(MarketScenarioTest, FactoryAlwaysReject_DefaultReason) {
    auto scenario = MarketScenario::alwaysReject();
    
    EXPECT_EQ(scenario.rejectReason, "Test rejection");
}

TEST_F(MarketScenarioTest, FactoryHighVolatility) {
    auto scenario = MarketScenario::highVolatility(280.0, 0.1);
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 280.0);
    EXPECT_DOUBLE_EQ(scenario.volatility, 0.1);
}

TEST_F(MarketScenarioTest, FactoryHighVolatility_DefaultVolatility) {
    auto scenario = MarketScenario::highVolatility(280.0);
    
    EXPECT_DOUBLE_EQ(scenario.volatility, 0.05);  // По умолчанию 5%
}

TEST_F(MarketScenarioTest, FactoryDelayed) {
    auto scenario = MarketScenario::delayed(280.0, std::chrono::milliseconds{200});
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 280.0);
    EXPECT_EQ(scenario.fillBehavior, OrderFillBehavior::DELAYED);
    EXPECT_EQ(scenario.executionDelay.count(), 200);
    EXPECT_EQ(scenario.fillDelay.count(), 200);
}

// ============================================================================
// BUILDER METHODS
// ============================================================================

TEST_F(MarketScenarioTest, BuilderMethods) {
    auto scenario = MarketScenario::realistic(280.0)
        .withVolatility(0.03)
        .withSpread(0.2)
        .withSlippage(0.005)
        .withLiquidity(5000)
        .withRejectProbability(0.1)
        .withRejectReason("Rate limit");
    
    EXPECT_DOUBLE_EQ(scenario.volatility, 0.03);
    EXPECT_DOUBLE_EQ(scenario.bidAskSpread, 0.2);
    EXPECT_DOUBLE_EQ(scenario.slippage, 0.005);
    EXPECT_EQ(scenario.availableLiquidity, 5000);
    EXPECT_DOUBLE_EQ(scenario.rejectProbability, 0.1);
    EXPECT_EQ(scenario.rejectReason, "Rate limit");
}

TEST_F(MarketScenarioTest, CustomConfiguration) {
    MarketScenario scenario;
    scenario.basePrice = 280.0;
    scenario.availableLiquidity = 500;
    scenario.slippagePercent = 0.02;
    scenario.fillBehavior = OrderFillBehavior::REALISTIC;
    scenario.fillDelay = std::chrono::milliseconds{100};
    scenario.rejectProbability = 0.05;
    scenario.rejectReason = "Rate limit exceeded";
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 280.0);
    EXPECT_EQ(scenario.availableLiquidity, 500);
    EXPECT_DOUBLE_EQ(scenario.slippagePercent, 0.02);
    EXPECT_EQ(scenario.fillDelay.count(), 100);
    EXPECT_DOUBLE_EQ(scenario.rejectProbability, 0.05);
    EXPECT_EQ(scenario.rejectReason, "Rate limit exceeded");
}

// ============================================================================
// COPY/MOVE SEMANTICS
// ============================================================================

TEST_F(MarketScenarioTest, CopySemantics) {
    auto original = MarketScenario::realistic(280.0, 0.002, 0.005);
    original.rejectReason = "Test";
    
    auto copy = original;
    
    EXPECT_DOUBLE_EQ(copy.basePrice, 280.0);
    EXPECT_EQ(copy.rejectReason, "Test");
}

TEST_F(MarketScenarioTest, MoveSemantics) {
    auto original = MarketScenario::realistic(280.0, 0.002, 0.005);
    original.rejectReason = "Test reason";
    
    auto moved = std::move(original);
    
    EXPECT_DOUBLE_EQ(moved.basePrice, 280.0);
    EXPECT_EQ(moved.rejectReason, "Test reason");
}

// ============================================================================
// ENUM CONVERSION
// ============================================================================

TEST_F(MarketScenarioTest, EnumToString) {
    EXPECT_EQ(toString(OrderFillBehavior::IMMEDIATE), "IMMEDIATE");
    EXPECT_EQ(toString(OrderFillBehavior::REALISTIC), "REALISTIC");
    EXPECT_EQ(toString(OrderFillBehavior::PARTIAL), "PARTIAL");
    EXPECT_EQ(toString(OrderFillBehavior::DELAYED), "DELAYED");
    EXPECT_EQ(toString(OrderFillBehavior::ALWAYS_REJECT), "ALWAYS_REJECT");
}

TEST_F(MarketScenarioTest, StringToEnum) {
    EXPECT_EQ(parseOrderFillBehavior("IMMEDIATE"), OrderFillBehavior::IMMEDIATE);
    EXPECT_EQ(parseOrderFillBehavior("REALISTIC"), OrderFillBehavior::REALISTIC);
    EXPECT_EQ(parseOrderFillBehavior("PARTIAL"), OrderFillBehavior::PARTIAL);
    EXPECT_EQ(parseOrderFillBehavior("DELAYED"), OrderFillBehavior::DELAYED);
    EXPECT_EQ(parseOrderFillBehavior("ALWAYS_REJECT"), OrderFillBehavior::ALWAYS_REJECT);
    EXPECT_EQ(parseOrderFillBehavior("UNKNOWN"), OrderFillBehavior::REALISTIC);  // Default
}
