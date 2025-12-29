#include <gtest/gtest.h>
#include "adapters/secondary/broker/OrderProcessor.hpp"
#include <thread>
#include <vector>

using namespace trading::adapters::secondary;

class OrderProcessorTest : public ::testing::Test {
protected:
    std::shared_ptr<PriceSimulator> priceSimulator;
    std::unique_ptr<OrderProcessor> processor;
    
    const std::string SBER_FIGI = "BBG004730N88";
    const std::string ACCOUNT_ID = "test-account-001";
    
    void SetUp() override {
        priceSimulator = std::make_shared<PriceSimulator>(42);  // Фиксированный seed
        priceSimulator->initInstrument(SBER_FIGI, 280.0, 0.001, 0.002);
        
        processor = std::make_unique<OrderProcessor>(priceSimulator);
    }
    
    OrderRequest makeMarketBuy(int64_t qty = 10) {
        OrderRequest req;
        req.accountId = ACCOUNT_ID;
        req.figi = SBER_FIGI;
        req.direction = Direction::BUY;
        req.type = Type::MARKET;
        req.quantity = qty;
        return req;
    }
    
    OrderRequest makeMarketSell(int64_t qty = 10) {
        OrderRequest req;
        req.accountId = ACCOUNT_ID;
        req.figi = SBER_FIGI;
        req.direction = Direction::SELL;
        req.type = Type::MARKET;
        req.quantity = qty;
        return req;
    }
    
    OrderRequest makeLimitBuy(double price, int64_t qty = 10) {
        OrderRequest req;
        req.accountId = ACCOUNT_ID;
        req.figi = SBER_FIGI;
        req.direction = Direction::BUY;
        req.type = Type::LIMIT;
        req.quantity = qty;
        req.price = price;
        return req;
    }
    
    OrderRequest makeLimitSell(double price, int64_t qty = 10) {
        OrderRequest req;
        req.accountId = ACCOUNT_ID;
        req.figi = SBER_FIGI;
        req.direction = Direction::SELL;
        req.type = Type::LIMIT;
        req.quantity = qty;
        req.price = price;
        return req;
    }
};

// ================================================================
// STATUS HELPERS
// ================================================================

TEST_F(OrderProcessorTest, ToString_Status) {
    EXPECT_EQ(toString(Status::PENDING), "PENDING");
    EXPECT_EQ(toString(Status::FILLED), "FILLED");
    EXPECT_EQ(toString(Status::PARTIALLY_FILLED), "PARTIALLY_FILLED");
    EXPECT_EQ(toString(Status::CANCELLED), "CANCELLED");
    EXPECT_EQ(toString(Status::REJECTED), "REJECTED");
}

TEST_F(OrderProcessorTest, OrderResult_IsSuccess) {
    OrderResult filled;
    filled.status = Status::FILLED;
    EXPECT_TRUE(filled.isSuccess());
    
    OrderResult partial;
    partial.status = Status::PARTIALLY_FILLED;
    EXPECT_TRUE(partial.isSuccess());
    
    OrderResult pending;
    pending.status = Status::PENDING;
    EXPECT_FALSE(pending.isSuccess());
    
    OrderResult rejected;
    rejected.status = Status::REJECTED;
    EXPECT_FALSE(rejected.isSuccess());
}

TEST_F(OrderProcessorTest, OrderResult_IsFinal) {
    OrderResult filled;
    filled.status = Status::FILLED;
    EXPECT_TRUE(filled.isFinal());
    
    OrderResult rejected;
    rejected.status = Status::REJECTED;
    EXPECT_TRUE(rejected.isFinal());
    
    OrderResult cancelled;
    cancelled.status = Status::CANCELLED;
    EXPECT_TRUE(cancelled.isFinal());
    
    OrderResult pending;
    pending.status = Status::PENDING;
    EXPECT_FALSE(pending.isFinal());
    
    OrderResult partial;
    partial.status = Status::PARTIALLY_FILLED;
    EXPECT_FALSE(partial.isFinal());
}

// ================================================================
// IMMEDIATE FILL BEHAVIOR
// ================================================================

TEST_F(OrderProcessorTest, Immediate_MarketBuy_Filled) {
    auto scenario = MarketScenario::immediate(280.0);
    auto req = makeMarketBuy(10);
    
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
    EXPECT_EQ(result.executedQuantity, 10);
    EXPECT_GT(result.executedPrice, 0.0);
    EXPECT_FALSE(result.orderId.empty());
}

TEST_F(OrderProcessorTest, Immediate_MarketSell_Filled) {
    auto scenario = MarketScenario::immediate(280.0);
    auto req = makeMarketSell(5);
    
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
    EXPECT_EQ(result.executedQuantity, 5);
}

TEST_F(OrderProcessorTest, Immediate_BuyAtAsk) {
    auto scenario = MarketScenario::immediate(280.0);
    auto req = makeMarketBuy(10);
    
    auto result = processor->processOrder(req, scenario);
    
    auto quote = priceSimulator->getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    
    // Buy должен исполняться по ask
    EXPECT_DOUBLE_EQ(result.executedPrice, quote->ask);
}

TEST_F(OrderProcessorTest, Immediate_SellAtBid) {
    auto scenario = MarketScenario::immediate(280.0);
    auto req = makeMarketSell(10);
    
    auto result = processor->processOrder(req, scenario);
    
    auto quote = priceSimulator->getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    
    // Sell должен исполняться по bid
    EXPECT_DOUBLE_EQ(result.executedPrice, quote->bid);
}

// ================================================================
// REALISTIC FILL BEHAVIOR - MARKET ORDERS
// ================================================================

TEST_F(OrderProcessorTest, Realistic_MarketOrder_FilledImmediately) {
    auto scenario = MarketScenario::realistic(280.0);
    auto req = makeMarketBuy(10);
    
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
    EXPECT_EQ(result.message, "Filled");
}

TEST_F(OrderProcessorTest, Realistic_MarketOrder_WithSlippage) {
    // Создаём сценарий с низкой ликвидностью
    auto scenario = MarketScenario::lowLiquidity(280.0, 100);
    
    // Большой ордер (50% ликвидности)
    auto req = makeMarketBuy(50);
    
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
    EXPECT_EQ(result.message, "Filled with slippage");
    
    // Цена должна быть выше ask из-за slippage
    auto quote = priceSimulator->getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    EXPECT_GT(result.executedPrice, quote->ask);
}

TEST_F(OrderProcessorTest, Realistic_MarketSell_WithSlippage) {
    auto scenario = MarketScenario::lowLiquidity(280.0, 100);
    auto req = makeMarketSell(50);
    
    auto result = processor->processOrder(req, scenario);
    
    auto quote = priceSimulator->getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    
    // Sell со slippage должен быть ниже bid
    EXPECT_LT(result.executedPrice, quote->bid);
}

TEST_F(OrderProcessorTest, Realistic_SmallOrder_NoSlippage) {
    auto scenario = MarketScenario::realistic(280.0);
    scenario.availableLiquidity = 10000;
    
    // Маленький ордер (< 10% ликвидности)
    auto req = makeMarketBuy(10);
    
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.message, "Filled");  // Без "with slippage"
}

// ================================================================
// REALISTIC FILL BEHAVIOR - LIMIT ORDERS
// ================================================================

TEST_F(OrderProcessorTest, Realistic_LimitBuy_BelowAsk_Pending) {
    auto scenario = MarketScenario::realistic(280.0);
    
    // Limit buy ниже текущей ask цены
    auto req = makeLimitBuy(275.0);  // ask ≈ 280.14
    
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::PENDING);
    EXPECT_EQ(result.message, "Limit order queued");
}

TEST_F(OrderProcessorTest, Realistic_LimitBuy_AboveAsk_FilledImmediately) {
    auto scenario = MarketScenario::realistic(280.0);
    
    // Limit buy выше текущей ask цены
    auto req = makeLimitBuy(285.0);  // ask ≈ 280.14
    
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
}

TEST_F(OrderProcessorTest, Realistic_LimitSell_AboveBid_Pending) {
    auto scenario = MarketScenario::realistic(280.0);
    
    // Limit sell выше текущей bid цены
    auto req = makeLimitSell(285.0);  // bid ≈ 279.86
    
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::PENDING);
}

TEST_F(OrderProcessorTest, Realistic_LimitSell_BelowBid_FilledImmediately) {
    auto scenario = MarketScenario::realistic(280.0);
    
    // Limit sell ниже текущей bid цены
    auto req = makeLimitSell(270.0);
    
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);
}

// ================================================================
// PENDING ORDERS PROCESSING
// ================================================================

TEST_F(OrderProcessorTest, ProcessPending_FillsWhenPriceReaches) {
    auto scenario = MarketScenario::realistic(280.0);
    
    // Размещаем limit buy ниже текущей цены
    auto req = makeLimitBuy(275.0);
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::PENDING);
    EXPECT_EQ(processor->pendingCount(), 1u);
    
    // Цена не достигла - ордер не исполняется
    int filled = processor->processPendingOrders();
    EXPECT_EQ(filled, 0);
    
    // Двигаем цену вниз (ask должен быть <= 275)
    priceSimulator->setPrice(SBER_FIGI, 274.0);
    
    // Теперь ордер должен исполниться
    filled = processor->processPendingOrders();
    EXPECT_EQ(filled, 1);
    EXPECT_EQ(processor->pendingCount(), 0u);
}

TEST_F(OrderProcessorTest, ProcessPending_SellFillsWhenPriceRises) {
    auto scenario = MarketScenario::realistic(280.0);
    
    // Limit sell выше текущей цены
    auto req = makeLimitSell(290.0);
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::PENDING);
    
    // Цена не достигла
    int filled = processor->processPendingOrders();
    EXPECT_EQ(filled, 0);
    
    // Двигаем цену вверх (bid должен быть >= 290)
    priceSimulator->setPrice(SBER_FIGI, 291.0);
    
    filled = processor->processPendingOrders();
    EXPECT_EQ(filled, 1);
}

TEST_F(OrderProcessorTest, ProcessPending_MultipleFills) {
    auto scenario = MarketScenario::realistic(280.0);
    
    // Размещаем несколько limit buy
    processor->processOrder(makeLimitBuy(275.0), scenario);
    processor->processOrder(makeLimitBuy(276.0), scenario);
    processor->processOrder(makeLimitBuy(277.0), scenario);
    
    EXPECT_EQ(processor->pendingCount(), 3u);
    
    // Цена падает до 274
    priceSimulator->setPrice(SBER_FIGI, 274.0);
    
    int filled = processor->processPendingOrders();
    EXPECT_EQ(filled, 3);
    EXPECT_EQ(processor->pendingCount(), 0u);
}

TEST_F(OrderProcessorTest, ProcessPending_FillCallback) {
    auto scenario = MarketScenario::realistic(280.0);
    
    std::vector<OrderFillEvent> events;
    processor->setFillCallback([&events](const OrderFillEvent& e) {
        events.push_back(e);
    });
    
    auto result = processor->processOrder(makeLimitBuy(275.0), scenario);
    EXPECT_EQ(events.size(), 0u);  // Ещё не исполнен
    
    priceSimulator->setPrice(SBER_FIGI, 274.0);
    processor->processPendingOrders();
    
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].orderId, result.orderId);
    EXPECT_EQ(events[0].figi, SBER_FIGI);
    EXPECT_GT(events[0].price, 0.0);
}

// ================================================================
// PARTIAL FILL
// ================================================================

TEST_F(OrderProcessorTest, Partial_FillsPartially) {
    auto scenario = MarketScenario::partialFill(280.0, 0.5);  // 50%
    
    auto req = makeMarketBuy(100);
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::PARTIALLY_FILLED);
    EXPECT_EQ(result.executedQuantity, 50);
}

TEST_F(OrderProcessorTest, Partial_MinimumOneLot) {
    auto scenario = MarketScenario::partialFill(280.0, 0.01);  // 1%
    
    auto req = makeMarketBuy(10);  // 1% от 10 = 0.1, но минимум 1
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_GE(result.executedQuantity, 1);
}

TEST_F(OrderProcessorTest, Partial_FullFillAtRatio1) {
    auto scenario = MarketScenario::partialFill(280.0, 1.0);  // 100%
    
    auto req = makeMarketBuy(10);
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::FILLED);  // Не PARTIALLY_FILLED
    EXPECT_EQ(result.executedQuantity, 10);
}

// ================================================================
// REJECTION
// ================================================================

TEST_F(OrderProcessorTest, AlwaysReject_MarketOrder) {
    auto scenario = MarketScenario::alwaysReject("Test rejection");
    
    auto req = makeMarketBuy(10);
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::REJECTED);
    EXPECT_EQ(result.message, "Test rejection");
}

TEST_F(OrderProcessorTest, AlwaysReject_LimitOrder) {
    auto scenario = MarketScenario::alwaysReject();
    
    auto req = makeLimitBuy(275.0);
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::REJECTED);
}

TEST_F(OrderProcessorTest, RejectProbability_100Percent) {
    MarketScenario scenario;
    scenario.rejectProbability = 1.0;
    scenario.rejectReason = "Always reject";
    
    auto req = makeMarketBuy(10);
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::REJECTED);
}

TEST_F(OrderProcessorTest, RejectProbability_0Percent) {
    auto scenario = MarketScenario::realistic(280.0);
    scenario.rejectProbability = 0.0;
    
    // Все ордера должны исполниться
    for (int i = 0; i < 100; ++i) {
        auto result = processor->processOrder(makeMarketBuy(1), scenario);
        EXPECT_EQ(result.status, Status::FILLED);
    }
}

TEST_F(OrderProcessorTest, Reject_InstrumentNotFound) {
    auto scenario = MarketScenario::realistic(280.0);
    
    OrderRequest req;
    req.figi = "UNKNOWN_FIGI";
    req.direction = Direction::BUY;
    req.type = Type::MARKET;
    req.quantity = 10;
    
    auto result = processor->processOrder(req, scenario);
    
    EXPECT_EQ(result.status, Status::REJECTED);
    EXPECT_TRUE(result.message.find("not found") != std::string::npos);
}

// ================================================================
// FORCE OPERATIONS
// ================================================================

TEST_F(OrderProcessorTest, ForceFillOrder_Success) {
    auto scenario = MarketScenario::realistic(280.0);
    auto result = processor->processOrder(makeLimitBuy(275.0), scenario);
    
    EXPECT_EQ(result.status, Status::PENDING);
    EXPECT_EQ(processor->pendingCount(), 1u);
    
    bool filled = processor->forceFillOrder(result.orderId, 274.0);
    
    EXPECT_TRUE(filled);
    EXPECT_EQ(processor->pendingCount(), 0u);
}

TEST_F(OrderProcessorTest, ForceFillOrder_NotFound) {
    bool filled = processor->forceFillOrder("non-existent", 274.0);
    EXPECT_FALSE(filled);
}

TEST_F(OrderProcessorTest, ForceRejectOrder_Success) {
    auto scenario = MarketScenario::realistic(280.0);
    auto result = processor->processOrder(makeLimitBuy(275.0), scenario);
    
    bool rejected = processor->forceRejectOrder(result.orderId);
    
    EXPECT_TRUE(rejected);
    EXPECT_EQ(processor->pendingCount(), 0u);
}

// ================================================================
// CANCEL OPERATIONS
// ================================================================

TEST_F(OrderProcessorTest, CancelOrder_Success) {
    auto scenario = MarketScenario::realistic(280.0);
    auto result = processor->processOrder(makeLimitBuy(275.0), scenario);
    
    bool cancelled = processor->cancelOrder(result.orderId);
    
    EXPECT_TRUE(cancelled);
    EXPECT_EQ(processor->pendingCount(), 0u);
}

TEST_F(OrderProcessorTest, CancelOrder_NotFound) {
    bool cancelled = processor->cancelOrder("non-existent");
    EXPECT_FALSE(cancelled);
}

TEST_F(OrderProcessorTest, GetPendingOrder_Found) {
    auto scenario = MarketScenario::realistic(280.0);
    auto result = processor->processOrder(makeLimitBuy(275.0), scenario);
    
    auto pending = processor->getPendingOrder(result.orderId);
    
    ASSERT_TRUE(pending.has_value());
    EXPECT_EQ(pending->orderId, result.orderId);
    EXPECT_EQ(pending->figi, SBER_FIGI);
    EXPECT_EQ(pending->direction, Direction::BUY);
    EXPECT_DOUBLE_EQ(pending->limitPrice, 275.0);
}

TEST_F(OrderProcessorTest, GetPendingOrder_NotFound) {
    auto pending = processor->getPendingOrder("non-existent");
    EXPECT_FALSE(pending.has_value());
}

// ================================================================
// CLEAR PENDING
// ================================================================

TEST_F(OrderProcessorTest, ClearPending_RemovesAll) {
    auto scenario = MarketScenario::realistic(280.0);
    
    processor->processOrder(makeLimitBuy(275.0), scenario);
    processor->processOrder(makeLimitBuy(276.0), scenario);
    processor->processOrder(makeLimitBuy(277.0), scenario);
    
    EXPECT_EQ(processor->pendingCount(), 3u);
    
    processor->clearPending();
    
    EXPECT_EQ(processor->pendingCount(), 0u);
}

// ================================================================
// DELAYED BEHAVIOR
// ================================================================

TEST_F(OrderProcessorTest, Delayed_GoesToPending) {
    MarketScenario scenario;
    scenario.fillBehavior = OrderFillBehavior::DELAYED;
    scenario.fillDelay = std::chrono::milliseconds{100};
    
    auto req = makeMarketBuy(10);
    auto result = processor->processOrder(req, scenario);
    
    // Delayed ордера пока идут в pending
    EXPECT_EQ(result.status, Status::PENDING);
}

// ================================================================
// ORDER ID UNIQUENESS
// ================================================================

TEST_F(OrderProcessorTest, OrderId_Unique) {
    auto scenario = MarketScenario::immediate(280.0);
    
    std::set<std::string> orderIds;
    
    for (int i = 0; i < 1000; ++i) {
        auto result = processor->processOrder(makeMarketBuy(1), scenario);
        EXPECT_TRUE(orderIds.insert(result.orderId).second)
            << "Duplicate order ID: " << result.orderId;
    }
}

// ================================================================
// THREAD SAFETY
// ================================================================

TEST_F(OrderProcessorTest, ThreadSafety_ConcurrentOrders) {
    auto scenario = MarketScenario::immediate(280.0);
    
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    const int ordersPerThread = 100;
    const int numThreads = 4;
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, &scenario, &successCount, ordersPerThread]() {
            for (int j = 0; j < ordersPerThread; ++j) {
                auto result = processor->processOrder(makeMarketBuy(1), scenario);
                if (result.status == Status::FILLED) {
                    ++successCount;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(successCount, numThreads * ordersPerThread);
}

TEST_F(OrderProcessorTest, ThreadSafety_ConcurrentPendingProcessing) {
    auto scenario = MarketScenario::realistic(280.0);
    
    // Добавляем много pending ордеров
    for (int i = 0; i < 100; ++i) {
        processor->processOrder(makeLimitBuy(275.0 - i * 0.01), scenario);
    }
    
    // Параллельно обрабатываем pending и двигаем цену
    std::atomic<bool> running{true};
    
    std::thread priceThread([this, &running]() {
        while (running) {
            priceSimulator->setPrice(SBER_FIGI, 270.0 + (rand() % 20));
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    std::thread processingThread([this, &running]() {
        while (running) {
            processor->processPendingOrders();
        }
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;
    
    priceThread.join();
    processingThread.join();
    
    // Не должно быть crashes или deadlocks
}
