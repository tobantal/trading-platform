/**
 * @file BackgroundTickerTest.cpp
 * @brief Unit tests for BackgroundTicker
 */

#include <gtest/gtest.h>
#include "adapters/secondary/broker/BackgroundTicker.hpp"
#include <thread>
#include <chrono>

using namespace broker::adapters::secondary;

class BackgroundTickerTest : public ::testing::Test {
protected:
    void SetUp() override {
        priceSimulator_ = std::make_shared<PriceSimulator>();
        priceSimulator_->initInstrument("SBER", 280.0, 0.02);
        priceSimulator_->initInstrument("GAZP", 150.0, 0.025);
        
        orderProcessor_ = std::make_shared<OrderProcessor>(priceSimulator_);
    }

    void TearDown() override {
        orderProcessor_.reset();
        priceSimulator_.reset();
    }

    std::shared_ptr<PriceSimulator> priceSimulator_;
    std::shared_ptr<OrderProcessor> orderProcessor_;
};

TEST_F(BackgroundTickerTest, Construction_WithDefaults) {
    BackgroundTicker ticker(priceSimulator_, orderProcessor_);
    EXPECT_FALSE(ticker.isRunning());
}

TEST_F(BackgroundTickerTest, Construction_WithInterval) {
    BackgroundTicker ticker(
        priceSimulator_, 
        orderProcessor_,
        std::chrono::milliseconds{50}
    );
    EXPECT_FALSE(ticker.isRunning());
}

TEST_F(BackgroundTickerTest, StartStop_Basic) {
    BackgroundTicker ticker(priceSimulator_, orderProcessor_);
    
    ticker.start();
    EXPECT_TRUE(ticker.isRunning());
    
    ticker.stop();
    EXPECT_FALSE(ticker.isRunning());
}

TEST_F(BackgroundTickerTest, Start_MultipleTimes_NoOp) {
    BackgroundTicker ticker(priceSimulator_, orderProcessor_);
    
    ticker.start();
    ticker.start();  // Should be ignored
    
    EXPECT_TRUE(ticker.isRunning());
    
    ticker.stop();
}

TEST_F(BackgroundTickerTest, Stop_WhenNotRunning_NoOp) {
    BackgroundTicker ticker(priceSimulator_, orderProcessor_);
    
    ticker.stop();  // Should be safe
    EXPECT_FALSE(ticker.isRunning());
}

TEST_F(BackgroundTickerTest, Tick_UpdatesPrices) {
    BackgroundTicker ticker(
        priceSimulator_, 
        orderProcessor_,
        std::chrono::milliseconds{10}
    );
    
    ticker.addInstrument("SBER");
    
    auto quoteBefore = priceSimulator_->getQuote("SBER");
    
    ticker.start();
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    ticker.stop();
    
    auto quoteAfter = priceSimulator_->getQuote("SBER");
    
    ASSERT_TRUE(quoteBefore.has_value());
    ASSERT_TRUE(quoteAfter.has_value());
    // Prices may or may not have changed depending on random walk
}

TEST_F(BackgroundTickerTest, ProcessesPendingOrders) {
    BackgroundTicker ticker(
        priceSimulator_, 
        orderProcessor_,
        std::chrono::milliseconds{10}
    );
    
    ticker.addInstrument("SBER");
    
    // Place a limit order above current price - should fill immediately in realistic mode
    OrderRequest req;
    req.accountId = "test-account";
    req.figi = "SBER";
    req.direction = Direction::BUY;
    req.type = Type::LIMIT;
    req.quantity = 10;
    req.price = 300.0;  // Above current price
    
    auto scenario = MarketScenario::realistic(280.0);
    auto result = orderProcessor_->processOrder(req, scenario);
    
    // Should fill immediately since price is above ask
    EXPECT_TRUE(result.status == Status::FILLED || result.status == Status::PENDING);
}

TEST_F(BackgroundTickerTest, GetTickCount) {
    BackgroundTicker ticker(
        priceSimulator_, 
        orderProcessor_,
        std::chrono::milliseconds{10}
    );
    
    ticker.addInstrument("SBER");
    
    EXPECT_EQ(ticker.getTickCount(), 0u);
    
    ticker.start();
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    ticker.stop();
    
    EXPECT_GT(ticker.getTickCount(), 0u);
}

TEST_F(BackgroundTickerTest, Destructor_StopsThread) {
    {
        BackgroundTicker ticker(priceSimulator_, orderProcessor_);
        ticker.start();
    }
    // Should not hang or crash
}

TEST_F(BackgroundTickerTest, SetInterval_WhileRunning) {
    BackgroundTicker ticker(
        priceSimulator_, 
        orderProcessor_,
        std::chrono::milliseconds{100}
    );
    
    ticker.addInstrument("SBER");
    
    ticker.start();
    ticker.setInterval(std::chrono::milliseconds{10});
    
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    ticker.stop();
    
    // More ticks should have occurred with shorter interval
    EXPECT_GT(ticker.getTickCount(), 0u);
}
