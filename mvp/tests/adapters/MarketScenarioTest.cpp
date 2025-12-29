#include <gtest/gtest.h>
#include "adapters/secondary/broker/MarketScenario.hpp"

using namespace trading::adapters::secondary;

class MarketScenarioTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

// ================================================================
// BASIC CONSTRUCTION
// ================================================================

TEST_F(MarketScenarioTest, DefaultValues) {
    MarketScenario scenario;
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 100.0);
    EXPECT_DOUBLE_EQ(scenario.bidAskSpread, 0.001);
    EXPECT_DOUBLE_EQ(scenario.volatility, 0.002);
    EXPECT_EQ(scenario.availableLiquidity, 10000);
    EXPECT_DOUBLE_EQ(scenario.slippagePercent, 0.001);
    EXPECT_EQ(scenario.fillBehavior, OrderFillBehavior::REALISTIC);
    EXPECT_EQ(scenario.fillDelay.count(), 0);
    EXPECT_DOUBLE_EQ(scenario.partialFillRatio, 1.0);
    EXPECT_DOUBLE_EQ(scenario.rejectProbability, 0.0);
    EXPECT_TRUE(scenario.rejectReason.empty());
}

TEST_F(MarketScenarioTest, ToString_FillBehavior) {
    EXPECT_EQ(toString(OrderFillBehavior::IMMEDIATE), "IMMEDIATE");
    EXPECT_EQ(toString(OrderFillBehavior::REALISTIC), "REALISTIC");
    EXPECT_EQ(toString(OrderFillBehavior::PARTIAL), "PARTIAL");
    EXPECT_EQ(toString(OrderFillBehavior::DELAYED), "DELAYED");
    EXPECT_EQ(toString(OrderFillBehavior::ALWAYS_REJECT), "ALWAYS_REJECT");
}

// ================================================================
// FACTORY METHODS
// ================================================================

TEST_F(MarketScenarioTest, FactoryImmediate) {
    auto scenario = MarketScenario::immediate(280.0);
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 280.0);
    EXPECT_EQ(scenario.fillBehavior, OrderFillBehavior::IMMEDIATE);
}

TEST_F(MarketScenarioTest, FactoryImmediate_DefaultPrice) {
    auto scenario = MarketScenario::immediate();
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 100.0);
    EXPECT_EQ(scenario.fillBehavior, OrderFillBehavior::IMMEDIATE);
}

TEST_F(MarketScenarioTest, FactoryRealistic) {
    auto scenario = MarketScenario::realistic(280.0, 0.002, 0.005);
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 280.0);
    EXPECT_DOUBLE_EQ(scenario.bidAskSpread, 0.002);
    EXPECT_DOUBLE_EQ(scenario.volatility, 0.005);
    EXPECT_EQ(scenario.fillBehavior, OrderFillBehavior::REALISTIC);
}

TEST_F(MarketScenarioTest, FactoryRealistic_DefaultSpreadAndVolatility) {
    auto scenario = MarketScenario::realistic(500.0);
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 500.0);
    EXPECT_DOUBLE_EQ(scenario.bidAskSpread, 0.001);
    EXPECT_DOUBLE_EQ(scenario.volatility, 0.002);
}

TEST_F(MarketScenarioTest, FactoryLowLiquidity) {
    auto scenario = MarketScenario::lowLiquidity(280.0, 50);
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 280.0);
    EXPECT_EQ(scenario.availableLiquidity, 50);
    EXPECT_DOUBLE_EQ(scenario.slippagePercent, 0.01);
    EXPECT_EQ(scenario.fillBehavior, OrderFillBehavior::REALISTIC);
}

TEST_F(MarketScenarioTest, FactoryLowLiquidity_DefaultLiquidity) {
    auto scenario = MarketScenario::lowLiquidity(280.0);
    
    EXPECT_EQ(scenario.availableLiquidity, 100);
}

TEST_F(MarketScenarioTest, FactoryPartialFill) {
    auto scenario = MarketScenario::partialFill(280.0, 0.3);
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 280.0);
    EXPECT_DOUBLE_EQ(scenario.partialFillRatio, 0.3);
    EXPECT_EQ(scenario.fillBehavior, OrderFillBehavior::PARTIAL);
}

TEST_F(MarketScenarioTest, FactoryPartialFill_DefaultRatio) {
    auto scenario = MarketScenario::partialFill(280.0);
    
    EXPECT_DOUBLE_EQ(scenario.partialFillRatio, 0.5);
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
    EXPECT_EQ(scenario.fillBehavior, OrderFillBehavior::REALISTIC);
}

TEST_F(MarketScenarioTest, FactoryHighVolatility_DefaultVolatility) {
    auto scenario = MarketScenario::highVolatility(280.0);
    
    EXPECT_DOUBLE_EQ(scenario.volatility, 0.05);
}

// ================================================================
// CUSTOM CONFIGURATION
// ================================================================

TEST_F(MarketScenarioTest, CustomConfiguration) {
    MarketScenario scenario;
    scenario.basePrice = 7200.0;
    scenario.bidAskSpread = 0.0005;
    scenario.volatility = 0.001;
    scenario.availableLiquidity = 500;
    scenario.slippagePercent = 0.02;
    scenario.fillBehavior = OrderFillBehavior::DELAYED;
    scenario.fillDelay = std::chrono::milliseconds{100};
    scenario.partialFillRatio = 0.8;
    scenario.rejectProbability = 0.05;
    scenario.rejectReason = "Rate limit exceeded";
    
    EXPECT_DOUBLE_EQ(scenario.basePrice, 7200.0);
    EXPECT_DOUBLE_EQ(scenario.bidAskSpread, 0.0005);
    EXPECT_DOUBLE_EQ(scenario.volatility, 0.001);
    EXPECT_EQ(scenario.availableLiquidity, 500);
    EXPECT_DOUBLE_EQ(scenario.slippagePercent, 0.02);
    EXPECT_EQ(scenario.fillBehavior, OrderFillBehavior::DELAYED);
    EXPECT_EQ(scenario.fillDelay.count(), 100);
    EXPECT_DOUBLE_EQ(scenario.partialFillRatio, 0.8);
    EXPECT_DOUBLE_EQ(scenario.rejectProbability, 0.05);
    EXPECT_EQ(scenario.rejectReason, "Rate limit exceeded");
}

// ================================================================
// COPY/MOVE SEMANTICS
// ================================================================

TEST_F(MarketScenarioTest, CopySemantics) {
    auto original = MarketScenario::realistic(280.0, 0.002, 0.005);
    original.rejectReason = "Test";
    
    MarketScenario copy = original;
    
    EXPECT_DOUBLE_EQ(copy.basePrice, 280.0);
    EXPECT_DOUBLE_EQ(copy.bidAskSpread, 0.002);
    EXPECT_EQ(copy.rejectReason, "Test");
    
    // Изменение копии не влияет на оригинал
    copy.basePrice = 300.0;
    EXPECT_DOUBLE_EQ(original.basePrice, 280.0);
}

TEST_F(MarketScenarioTest, MoveSemantics) {
    auto original = MarketScenario::realistic(280.0, 0.002, 0.005);
    original.rejectReason = "Test reason";
    
    MarketScenario moved = std::move(original);
    
    EXPECT_DOUBLE_EQ(moved.basePrice, 280.0);
    EXPECT_EQ(moved.rejectReason, "Test reason");
}
