#include <gtest/gtest.h>
#include "adapters/secondary/broker/BackgroundTicker.hpp"
#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <thread>

using namespace trading::adapters::secondary;
using namespace std::chrono_literals;

class BackgroundTickerTest : public ::testing::Test {
protected:
    std::shared_ptr<PriceSimulator> priceSimulator;
    std::shared_ptr<OrderProcessor> orderProcessor;
    
    const std::string SBER_FIGI = "BBG004730N88";
    const std::string GAZP_FIGI = "BBG004730RP0";
    
    void SetUp() override {
        priceSimulator = std::make_shared<PriceSimulator>(42);
        orderProcessor = std::make_shared<OrderProcessor>(priceSimulator);
        
        priceSimulator->initInstrument(SBER_FIGI, 280.0, 0.001, 0.01);
        priceSimulator->initInstrument(GAZP_FIGI, 160.0, 0.001, 0.01);
    }
};

// ================================================================
// BASIC LIFECYCLE
// ================================================================

TEST_F(BackgroundTickerTest, StartStop_Basic) {
    BackgroundTicker ticker(priceSimulator, orderProcessor);
    
    EXPECT_FALSE(ticker.isRunning());
    
    ticker.start(50ms);
    EXPECT_TRUE(ticker.isRunning());
    
    std::this_thread::sleep_for(100ms);
    
    ticker.stop();
    EXPECT_FALSE(ticker.isRunning());
}

TEST_F(BackgroundTickerTest, DoubleStart_NoOp) {
    BackgroundTicker ticker(priceSimulator);
    
    ticker.start(50ms);
    ticker.start(50ms);  // Второй вызов игнорируется
    
    EXPECT_TRUE(ticker.isRunning());
    
    ticker.stop();
}

TEST_F(BackgroundTickerTest, DoubleStop_NoOp) {
    BackgroundTicker ticker(priceSimulator);
    
    ticker.start(50ms);
    ticker.stop();
    ticker.stop();  // Второй вызов игнорируется
    
    EXPECT_FALSE(ticker.isRunning());
}

TEST_F(BackgroundTickerTest, DestructorStops) {
    auto ticker = std::make_unique<BackgroundTicker>(priceSimulator);
    ticker->start(50ms);
    EXPECT_TRUE(ticker->isRunning());
    
    ticker.reset();  // Деструктор должен остановить
    // Не должно быть утечек или зависаний
}

// ================================================================
// TICK COUNTING
// ================================================================

TEST_F(BackgroundTickerTest, TickCount_Increments) {
    BackgroundTicker ticker(priceSimulator);
    ticker.addInstrument(SBER_FIGI);
    
    EXPECT_EQ(ticker.tickCount(), 0u);
    
    ticker.start(10ms);
    std::this_thread::sleep_for(100ms);
    ticker.stop();
    
    // За 100ms с интервалом 10ms должно быть ~10 тиков
    EXPECT_GE(ticker.tickCount(), 5u);
    EXPECT_LE(ticker.tickCount(), 15u);
}

TEST_F(BackgroundTickerTest, ManualTick_IncrementsToo) {
    BackgroundTicker ticker(priceSimulator);
    ticker.addInstrument(SBER_FIGI);
    
    EXPECT_EQ(ticker.tickCount(), 0u);
    
    ticker.manualTick();
    EXPECT_EQ(ticker.tickCount(), 1u);
    
    ticker.manualTick();
    ticker.manualTick();
    EXPECT_EQ(ticker.tickCount(), 3u);
}

// ================================================================
// INSTRUMENT MANAGEMENT
// ================================================================

TEST_F(BackgroundTickerTest, AddRemoveInstrument) {
    BackgroundTicker ticker(priceSimulator);
    
    ticker.addInstrument(SBER_FIGI);
    ticker.addInstrument(GAZP_FIGI);
    
    // Ничего не крашится при тике
    ticker.manualTick();
    
    ticker.removeInstrument(SBER_FIGI);
    ticker.manualTick();
}

// ================================================================
// PRICE CHANGES
// ================================================================

TEST_F(BackgroundTickerTest, TickChangesPrices) {
    BackgroundTicker ticker(priceSimulator);
    ticker.addInstrument(SBER_FIGI);
    
    double originalPrice = priceSimulator->getPrice(SBER_FIGI);
    
    // Много тиков
    for (int i = 0; i < 100; ++i) {
        ticker.manualTick();
    }
    
    double newPrice = priceSimulator->getPrice(SBER_FIGI);
    EXPECT_NE(newPrice, originalPrice);
}

// ================================================================
// QUOTE CALLBACK
// ================================================================

TEST_F(BackgroundTickerTest, QuoteCallback_Called) {
    BackgroundTicker ticker(priceSimulator);
    ticker.addInstrument(SBER_FIGI);
    
    std::atomic<int> callCount{0};
    std::string lastFigi;
    
    ticker.setQuoteCallback([&](const QuoteUpdate& q) {
        ++callCount;
        lastFigi = q.figi;
    });
    
    ticker.manualTick();
    
    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(lastFigi, SBER_FIGI);
}

TEST_F(BackgroundTickerTest, QuoteCallback_CalledForEachInstrument) {
    BackgroundTicker ticker(priceSimulator);
    ticker.addInstrument(SBER_FIGI);
    ticker.addInstrument(GAZP_FIGI);
    
    std::atomic<int> callCount{0};
    std::set<std::string> seenFigis;
    std::mutex seenMutex;
    
    ticker.setQuoteCallback([&](const QuoteUpdate& q) {
        ++callCount;
        std::lock_guard<std::mutex> lock(seenMutex);
        seenFigis.insert(q.figi);
    });
    
    ticker.manualTick();
    
    EXPECT_EQ(callCount, 2);
    EXPECT_EQ(seenFigis.size(), 2u);
    EXPECT_TRUE(seenFigis.count(SBER_FIGI) > 0);
    EXPECT_TRUE(seenFigis.count(GAZP_FIGI) > 0);
}

TEST_F(BackgroundTickerTest, QuoteCallback_ContainsValidData) {
    BackgroundTicker ticker(priceSimulator);
    ticker.addInstrument(SBER_FIGI);
    
    QuoteUpdate received;
    
    ticker.setQuoteCallback([&](const QuoteUpdate& q) {
        received = q;
    });
    
    ticker.manualTick();
    
    EXPECT_EQ(received.figi, SBER_FIGI);
    EXPECT_GT(received.bid, 0.0);
    EXPECT_GT(received.ask, 0.0);
    EXPECT_GT(received.last, 0.0);
    EXPECT_LE(received.bid, received.ask);  // bid <= ask
}

TEST_F(BackgroundTickerTest, QuoteCallback_AsyncCalls) {
    BackgroundTicker ticker(priceSimulator);
    ticker.addInstrument(SBER_FIGI);
    
    std::atomic<int> callCount{0};
    
    ticker.setQuoteCallback([&](const QuoteUpdate& q) {
        ++callCount;
    });
    
    ticker.start(10ms);
    std::this_thread::sleep_for(100ms);
    ticker.stop();
    
    EXPECT_GE(callCount, 5);
}

// ================================================================
// PENDING ORDERS PROCESSING
// ================================================================

TEST_F(BackgroundTickerTest, ProcessesPendingOrders) {
    BackgroundTicker ticker(priceSimulator, orderProcessor);
    ticker.addInstrument(SBER_FIGI);
    
    // Размещаем limit buy ниже текущей цены
    OrderRequest req;
    req.accountId = "acc-001";
    req.figi = SBER_FIGI;
    req.direction = Direction::BUY;
    req.type = Type::LIMIT;
    req.quantity = 10;
    req.price = 270.0;  // Ниже текущей 280
    
    auto scenario = MarketScenario::realistic(280.0);
    auto result = orderProcessor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::PENDING);
    EXPECT_EQ(orderProcessor->pendingCount(), 1u);
    
    // Двигаем цену вниз
    priceSimulator->setPrice(SBER_FIGI, 265.0);
    
    // Тик должен обработать pending
    ticker.manualTick();
    
    EXPECT_EQ(orderProcessor->pendingCount(), 0u);
}

TEST_F(BackgroundTickerTest, FillCallback_OnPendingFill) {
    BackgroundTicker ticker(priceSimulator, orderProcessor);
    ticker.addInstrument(SBER_FIGI);
    
    std::atomic<bool> fillCalled{false};
    std::string filledOrderId;
    
    orderProcessor->setFillCallback([&](const OrderFillEvent& e) {
        fillCalled = true;
        filledOrderId = e.orderId;
    });
    
    // Limit order
    OrderRequest req;
    req.accountId = "acc-001";
    req.figi = SBER_FIGI;
    req.direction = Direction::BUY;
    req.type = Type::LIMIT;
    req.quantity = 10;
    req.price = 270.0;
    
    auto result = orderProcessor->processOrder(req, MarketScenario::realistic(280.0));
    
    // Move price down and tick
    priceSimulator->setPrice(SBER_FIGI, 265.0);
    ticker.manualTick();
    
    EXPECT_TRUE(fillCalled);
    EXPECT_EQ(filledOrderId, result.orderId);
}

// ================================================================
// INTERVAL CONTROL
// ================================================================

TEST_F(BackgroundTickerTest, SetInterval_Changes) {
    BackgroundTicker ticker(priceSimulator);
    
    EXPECT_EQ(ticker.interval(), 100ms);
    
    ticker.setInterval(50ms);
    EXPECT_EQ(ticker.interval(), 50ms);
}

TEST_F(BackgroundTickerTest, SetInterval_WhileRunning) {
    BackgroundTicker ticker(priceSimulator);
    ticker.addInstrument(SBER_FIGI);
    
    ticker.start(100ms);
    
    auto count1 = ticker.tickCount();
    std::this_thread::sleep_for(200ms);
    auto count2 = ticker.tickCount();
    
    // Меняем интервал на более быстрый
    ticker.setInterval(10ms);
    
    std::this_thread::sleep_for(200ms);
    auto count3 = ticker.tickCount();
    
    ticker.stop();
    
    // После уменьшения интервала должно быть больше тиков
    auto ticksBeforeChange = count2 - count1;
    auto ticksAfterChange = count3 - count2;
    
    EXPECT_GT(ticksAfterChange, ticksBeforeChange);
}

// ================================================================
// WITHOUT ORDER PROCESSOR
// ================================================================

TEST_F(BackgroundTickerTest, WorksWithoutOrderProcessor) {
    BackgroundTicker ticker(priceSimulator);  // Без orderProcessor
    ticker.addInstrument(SBER_FIGI);
    
    // Не должно крашиться
    ticker.manualTick();
    
    ticker.start(50ms);
    std::this_thread::sleep_for(100ms);
    ticker.stop();
}

// ================================================================
// THREAD SAFETY
// ================================================================

TEST_F(BackgroundTickerTest, ThreadSafe_AddRemoveWhileRunning) {
    BackgroundTicker ticker(priceSimulator);
    ticker.addInstrument(SBER_FIGI);
    
    ticker.start(10ms);
    
    // Add/remove instruments while running
    for (int i = 0; i < 100; ++i) {
        ticker.addInstrument("INSTR" + std::to_string(i));
        if (i > 0) {
            ticker.removeInstrument("INSTR" + std::to_string(i - 1));
        }
    }
    
    ticker.stop();
}

TEST_F(BackgroundTickerTest, ThreadSafe_CallbackChange) {
    BackgroundTicker ticker(priceSimulator);
    ticker.addInstrument(SBER_FIGI);
    
    std::atomic<int> count1{0};
    std::atomic<int> count2{0};
    
    ticker.setQuoteCallback([&](const QuoteUpdate&) { ++count1; });
    
    ticker.start(10ms);
    std::this_thread::sleep_for(50ms);
    
    // Change callback while running
    ticker.setQuoteCallback([&](const QuoteUpdate&) { ++count2; });
    
    std::this_thread::sleep_for(50ms);
    ticker.stop();
    
    EXPECT_GT(count1, 0);
    EXPECT_GT(count2, 0);
}
