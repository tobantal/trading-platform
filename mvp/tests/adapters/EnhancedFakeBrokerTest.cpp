#include <gtest/gtest.h>
#include "adapters/secondary/broker/EnhancedFakeBroker.hpp"
#include <atomic>
#include <chrono>
#include <thread>

using namespace trading::adapters::secondary;
using namespace std::chrono_literals;

class EnhancedFakeBrokerTest : public ::testing::Test {
protected:
    std::unique_ptr<EnhancedFakeBroker> broker;
    
    const std::string ACCOUNT_ID = "test-account-001";
    const std::string SBER_FIGI = "BBG004730N88";
    const std::string GAZP_FIGI = "BBG004730RP0";
    const std::string LKOH_FIGI = "BBG004731032";
    
    void SetUp() override {
        broker = std::make_unique<EnhancedFakeBroker>(42);  // Fixed seed
        broker->registerAccount(ACCOUNT_ID, "test-token", 1000000.0);
    }
    
    void TearDown() override {
        broker->stopSimulation();
    }
    
    BrokerOrderRequest makeMarketBuy(const std::string& figi, int64_t qty = 10) {
        BrokerOrderRequest req;
        req.accountId = ACCOUNT_ID;
        req.figi = figi;
        req.direction = Direction::BUY;
        req.type = Type::MARKET;
        req.quantity = qty;
        return req;
    }
    
    BrokerOrderRequest makeMarketSell(const std::string& figi, int64_t qty = 10) {
        BrokerOrderRequest req;
        req.accountId = ACCOUNT_ID;
        req.figi = figi;
        req.direction = Direction::SELL;
        req.type = Type::MARKET;
        req.quantity = qty;
        return req;
    }
    
    BrokerOrderRequest makeLimitBuy(const std::string& figi, double price, int64_t qty = 10) {
        BrokerOrderRequest req;
        req.accountId = ACCOUNT_ID;
        req.figi = figi;
        req.direction = Direction::BUY;
        req.type = Type::LIMIT;
        req.quantity = qty;
        req.price = price;
        return req;
    }
};

// ================================================================
// INITIALIZATION
// ================================================================

TEST_F(EnhancedFakeBrokerTest, DefaultInstruments_Exist) {
    auto instruments = broker->getAllInstruments();
    EXPECT_EQ(instruments.size(), 5u);
    
    auto sber = broker->getInstrument(SBER_FIGI);
    ASSERT_TRUE(sber.has_value());
    EXPECT_EQ(sber->ticker, "SBER");
    EXPECT_EQ(sber->lot, 10);
}

TEST_F(EnhancedFakeBrokerTest, DefaultQuotes_Available) {
    auto quote = broker->getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    EXPECT_EQ(quote->figi, SBER_FIGI);
    EXPECT_EQ(quote->ticker, "SBER");
    EXPECT_NEAR(quote->lastPrice, 280.0, 10.0);
}

// ================================================================
// ACCOUNT MANAGEMENT
// ================================================================

TEST_F(EnhancedFakeBrokerTest, RegisterAccount) {
    broker->registerAccount("new-account", "token", 500000.0);
    EXPECT_TRUE(broker->hasAccount("new-account"));
    
    auto portfolio = broker->getPortfolio("new-account");
    ASSERT_TRUE(portfolio.has_value());
    EXPECT_DOUBLE_EQ(portfolio->cash, 500000.0);
}

TEST_F(EnhancedFakeBrokerTest, UnregisterAccount) {
    broker->registerAccount("temp-account", "token");
    EXPECT_TRUE(broker->hasAccount("temp-account"));
    
    broker->unregisterAccount("temp-account");
    EXPECT_FALSE(broker->hasAccount("temp-account"));
}

TEST_F(EnhancedFakeBrokerTest, SetCash) {
    broker->setCash(ACCOUNT_ID, 123456.0);
    
    auto portfolio = broker->getPortfolio(ACCOUNT_ID);
    ASSERT_TRUE(portfolio.has_value());
    EXPECT_DOUBLE_EQ(portfolio->cash, 123456.0);
}

// ================================================================
// MARKET DATA
// ================================================================

TEST_F(EnhancedFakeBrokerTest, GetQuotes_Multiple) {
    auto quotes = broker->getQuotes({SBER_FIGI, GAZP_FIGI, LKOH_FIGI});
    EXPECT_EQ(quotes.size(), 3u);
}

TEST_F(EnhancedFakeBrokerTest, GetQuote_Unknown_ReturnsNullopt) {
    auto quote = broker->getQuote("UNKNOWN_FIGI");
    EXPECT_FALSE(quote.has_value());
}

TEST_F(EnhancedFakeBrokerTest, SearchInstruments) {
    auto results = broker->searchInstruments("SBER");
    EXPECT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].ticker, "SBER");
    
    results = broker->searchInstruments("gazp");  // Case insensitive
    EXPECT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].figi, GAZP_FIGI);
}

// ================================================================
// PRICE MANIPULATION
// ================================================================

TEST_F(EnhancedFakeBrokerTest, SetPrice) {
    broker->setPrice(SBER_FIGI, 300.0);
    
    auto quote = broker->getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    EXPECT_NEAR(quote->lastPrice, 300.0, 1.0);
}

TEST_F(EnhancedFakeBrokerTest, MovePrice) {
    broker->setPrice(SBER_FIGI, 280.0);
    broker->movePrice(SBER_FIGI, 10.0);
    
    auto quote = broker->getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    EXPECT_NEAR(quote->lastPrice, 290.0, 1.0);
}

TEST_F(EnhancedFakeBrokerTest, MovePricePercent) {
    broker->setPrice(SBER_FIGI, 100.0);
    broker->movePricePercent(SBER_FIGI, 10.0);  // +10%
    
    auto quote = broker->getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    EXPECT_NEAR(quote->lastPrice, 110.0, 1.0);
}

// ================================================================
// ORDERS - BASIC
// ================================================================

TEST_F(EnhancedFakeBrokerTest, MarketBuy_Success) {
    auto result = broker->placeOrder(ACCOUNT_ID, makeMarketBuy(SBER_FIGI, 10));
    
    EXPECT_EQ(result.status, Status::FILLED);
    EXPECT_EQ(result.executedQuantity, 10);
    EXPECT_GT(result.executedPrice, 0.0);
    EXPECT_FALSE(result.orderId.empty());
}

TEST_F(EnhancedFakeBrokerTest, MarketBuy_UpdatesPortfolio) {
    double initialCash = broker->getPortfolio(ACCOUNT_ID)->cash;
    
    auto result = broker->placeOrder(ACCOUNT_ID, makeMarketBuy(SBER_FIGI, 10));
    EXPECT_EQ(result.status, Status::FILLED);
    
    auto portfolio = broker->getPortfolio(ACCOUNT_ID);
    ASSERT_TRUE(portfolio.has_value());
    
    // Cash уменьшился
    EXPECT_LT(portfolio->cash, initialCash);
    
    // Позиция появилась (10 лотов * 10 акций = 100 акций)
    EXPECT_EQ(portfolio->positions.size(), 1u);
    EXPECT_EQ(portfolio->positions[0].figi, SBER_FIGI);
    EXPECT_EQ(portfolio->positions[0].quantity, 100);  // 10 lots * 10 shares/lot
}

TEST_F(EnhancedFakeBrokerTest, MarketSell_Success) {
    // Сначала покупаем
    broker->placeOrder(ACCOUNT_ID, makeMarketBuy(SBER_FIGI, 20));
    
    // Потом продаём
    auto result = broker->placeOrder(ACCOUNT_ID, makeMarketSell(SBER_FIGI, 10));
    
    EXPECT_EQ(result.status, Status::FILLED);
    EXPECT_EQ(result.executedQuantity, 10);
    
    auto portfolio = broker->getPortfolio(ACCOUNT_ID);
    EXPECT_EQ(portfolio->positions[0].quantity, 100);  // Осталось 10 лотов = 100 акций
}

TEST_F(EnhancedFakeBrokerTest, MarketSell_AllPosition) {
    broker->placeOrder(ACCOUNT_ID, makeMarketBuy(SBER_FIGI, 10));
    broker->placeOrder(ACCOUNT_ID, makeMarketSell(SBER_FIGI, 10));
    
    auto portfolio = broker->getPortfolio(ACCOUNT_ID);
    EXPECT_TRUE(portfolio->positions.empty());
}

// ================================================================
// ORDERS - REJECTION
// ================================================================

TEST_F(EnhancedFakeBrokerTest, MarketBuy_InsufficientFunds) {
    broker->setCash(ACCOUNT_ID, 100.0);  // Мало денег
    
    auto result = broker->placeOrder(ACCOUNT_ID, makeMarketBuy(SBER_FIGI, 10));
    
    EXPECT_EQ(result.status, Status::REJECTED);
    EXPECT_TRUE(result.message.find("Insufficient funds") != std::string::npos);
}

TEST_F(EnhancedFakeBrokerTest, MarketSell_InsufficientPosition) {
    auto result = broker->placeOrder(ACCOUNT_ID, makeMarketSell(SBER_FIGI, 10));
    
    EXPECT_EQ(result.status, Status::REJECTED);
    EXPECT_TRUE(result.message.find("Insufficient position") != std::string::npos);
}

TEST_F(EnhancedFakeBrokerTest, Order_UnknownAccount) {
    BrokerOrderRequest req = makeMarketBuy(SBER_FIGI);
    req.accountId = "non-existent";
    
    auto result = broker->placeOrder("non-existent", req);
    
    EXPECT_EQ(result.status, Status::REJECTED);
    EXPECT_TRUE(result.message.find("Account not found") != std::string::npos);
}

TEST_F(EnhancedFakeBrokerTest, Order_UnknownInstrument) {
    BrokerOrderRequest req = makeMarketBuy("UNKNOWN_FIGI");
    
    auto result = broker->placeOrder(ACCOUNT_ID, req);
    
    EXPECT_EQ(result.status, Status::REJECTED);
    EXPECT_TRUE(result.message.find("Instrument not found") != std::string::npos);
}

// ================================================================
// ORDERS - LIMIT
// ================================================================

TEST_F(EnhancedFakeBrokerTest, LimitBuy_BelowAsk_Pending) {
    auto result = broker->placeOrder(ACCOUNT_ID, makeLimitBuy(SBER_FIGI, 270.0));
    
    EXPECT_EQ(result.status, Status::PENDING);
    EXPECT_EQ(broker->pendingOrderCount(), 1u);
}

TEST_F(EnhancedFakeBrokerTest, LimitBuy_AboveAsk_FilledImmediately) {
    auto result = broker->placeOrder(ACCOUNT_ID, makeLimitBuy(SBER_FIGI, 300.0));
    
    EXPECT_EQ(result.status, Status::FILLED);
    EXPECT_EQ(broker->pendingOrderCount(), 0u);
}

TEST_F(EnhancedFakeBrokerTest, LimitBuy_FillsWhenPriceDrops) {
    auto result = broker->placeOrder(ACCOUNT_ID, makeLimitBuy(SBER_FIGI, 270.0));
    EXPECT_EQ(result.status, Status::PENDING);
    
    // Двигаем цену вниз
    broker->setPrice(SBER_FIGI, 265.0);
    
    // Ручной тик
    broker->manualTick();
    
    EXPECT_EQ(broker->pendingOrderCount(), 0u);
}

// ================================================================
// SCENARIOS
// ================================================================

TEST_F(EnhancedFakeBrokerTest, SetScenario_Slippage) {
    auto scenario = MarketScenario::lowLiquidity(280.0, 100);
    broker->setScenario(SBER_FIGI, scenario);
    
    // Большой ордер должен иметь slippage
    auto result = broker->placeOrder(ACCOUNT_ID, makeMarketBuy(SBER_FIGI, 50));
    
    EXPECT_EQ(result.status, Status::FILLED);
    EXPECT_TRUE(result.message.find("slippage") != std::string::npos);
}

TEST_F(EnhancedFakeBrokerTest, SetScenario_PartialFill) {
    auto scenario = MarketScenario::partialFill(280.0, 0.5);
    broker->setScenario(SBER_FIGI, scenario);
    
    auto result = broker->placeOrder(ACCOUNT_ID, makeMarketBuy(SBER_FIGI, 100));
    
    EXPECT_EQ(result.status, Status::PARTIALLY_FILLED);
    EXPECT_EQ(result.executedQuantity, 50);
}

TEST_F(EnhancedFakeBrokerTest, SetScenario_AlwaysReject) {
    auto scenario = MarketScenario::alwaysReject("Market closed");
    broker->setScenario(SBER_FIGI, scenario);
    
    auto result = broker->placeOrder(ACCOUNT_ID, makeMarketBuy(SBER_FIGI));
    
    EXPECT_EQ(result.status, Status::REJECTED);
    EXPECT_EQ(result.message, "Market closed");
}

// ================================================================
// SIMULATION
// ================================================================

TEST_F(EnhancedFakeBrokerTest, Simulation_StartStop) {
    EXPECT_FALSE(broker->isSimulationRunning());
    
    broker->startSimulation(50ms);
    EXPECT_TRUE(broker->isSimulationRunning());
    
    broker->stopSimulation();
    EXPECT_FALSE(broker->isSimulationRunning());
}

TEST_F(EnhancedFakeBrokerTest, Simulation_PricesChange) {
    auto quote1 = broker->getQuote(SBER_FIGI);
    double price1 = quote1->lastPrice;
    
    broker->startSimulation(10ms);
    std::this_thread::sleep_for(200ms);
    broker->stopSimulation();
    
    auto quote2 = broker->getQuote(SBER_FIGI);
    double price2 = quote2->lastPrice;
    
    // Цена должна измениться
    EXPECT_NE(price1, price2);
}

// ================================================================
// CALLBACKS
// ================================================================

TEST_F(EnhancedFakeBrokerTest, QuoteUpdateCallback) {
    std::atomic<int> callCount{0};
    
    broker->setQuoteUpdateCallback([&](const BrokerQuoteUpdateEvent& e) {
        ++callCount;
    });
    
    broker->manualTick();
    
    // Должен вызваться для каждого инструмента (5 по умолчанию)
    EXPECT_EQ(callCount, 5);
}

TEST_F(EnhancedFakeBrokerTest, OrderFillCallback_OnLimitFill) {
    std::atomic<bool> fillCalled{false};
    std::string filledOrderId;
    
    broker->setOrderFillCallback([&](const BrokerOrderFillEvent& e) {
        fillCalled = true;
        filledOrderId = e.orderId;
    });
    
    auto result = broker->placeOrder(ACCOUNT_ID, makeLimitBuy(SBER_FIGI, 270.0));
    EXPECT_EQ(result.status, Status::PENDING);
    EXPECT_FALSE(fillCalled);
    
    // Двигаем цену и тикаем
    broker->setPrice(SBER_FIGI, 265.0);
    broker->manualTick();
    
    EXPECT_TRUE(fillCalled);
    EXPECT_EQ(filledOrderId, result.orderId);
}

// ================================================================
// PORTFOLIO CALCULATIONS
// ================================================================

TEST_F(EnhancedFakeBrokerTest, Portfolio_TotalValue) {
    broker->setCash(ACCOUNT_ID, 100000.0);
    
    // Покупаем акций на ~28000 (10 лотов * 10 акций * 280)
    broker->placeOrder(ACCOUNT_ID, makeMarketBuy(SBER_FIGI, 10));
    
    auto portfolio = broker->getPortfolio(ACCOUNT_ID);
    ASSERT_TRUE(portfolio.has_value());
    
    // Total = cash + positions
    double expectedMin = 90000.0;  // cash должен быть около 72000
    double expectedMax = 110000.0;
    EXPECT_GT(portfolio->totalValue(), expectedMin);
    EXPECT_LT(portfolio->totalValue(), expectedMax);
}

TEST_F(EnhancedFakeBrokerTest, Position_PnL) {
    broker->setPrice(SBER_FIGI, 100.0);  // Упростим для расчёта
    broker->placeOrder(ACCOUNT_ID, makeMarketBuy(SBER_FIGI, 10));
    
    // Цена выросла на 10%
    broker->setPrice(SBER_FIGI, 110.0);
    
    auto portfolio = broker->getPortfolio(ACCOUNT_ID);
    ASSERT_TRUE(portfolio.has_value());
    ASSERT_EQ(portfolio->positions.size(), 1u);
    
    auto& pos = portfolio->positions[0];
    EXPECT_GT(pos.pnl(), 0.0);  // Должна быть прибыль
    EXPECT_NEAR(pos.pnlPercent(), 10.0, 2.0);  // Около 10%
}

// ================================================================
// RESET
// ================================================================

TEST_F(EnhancedFakeBrokerTest, Reset_ClearsState) {
    broker->placeOrder(ACCOUNT_ID, makeMarketBuy(SBER_FIGI, 10));
    broker->placeOrder(ACCOUNT_ID, makeLimitBuy(SBER_FIGI, 270.0));
    
    broker->reset();
    
    EXPECT_FALSE(broker->hasAccount(ACCOUNT_ID));
    EXPECT_EQ(broker->pendingOrderCount(), 0u);
    EXPECT_FALSE(broker->isSimulationRunning());
    
    // Инструменты должны пересоздаться
    EXPECT_EQ(broker->getAllInstruments().size(), 5u);
}

// ================================================================
// CANCEL ORDER
// ================================================================

TEST_F(EnhancedFakeBrokerTest, CancelOrder_Success) {
    auto result = broker->placeOrder(ACCOUNT_ID, makeLimitBuy(SBER_FIGI, 270.0));
    EXPECT_EQ(result.status, Status::PENDING);
    EXPECT_EQ(broker->pendingOrderCount(), 1u);
    
    bool cancelled = broker->cancelOrder(ACCOUNT_ID, result.orderId);
    
    EXPECT_TRUE(cancelled);
    EXPECT_EQ(broker->pendingOrderCount(), 0u);
}

TEST_F(EnhancedFakeBrokerTest, CancelOrder_NotFound) {
    bool cancelled = broker->cancelOrder(ACCOUNT_ID, "non-existent");
    EXPECT_FALSE(cancelled);
}

// ================================================================
// MULTIPLE ORDERS
// ================================================================

TEST_F(EnhancedFakeBrokerTest, MultipleOrders_DifferentInstruments) {
    broker->placeOrder(ACCOUNT_ID, makeMarketBuy(SBER_FIGI, 10));
    broker->placeOrder(ACCOUNT_ID, makeMarketBuy(GAZP_FIGI, 5));
    broker->placeOrder(ACCOUNT_ID, makeMarketBuy(LKOH_FIGI, 1));
    
    auto portfolio = broker->getPortfolio(ACCOUNT_ID);
    ASSERT_TRUE(portfolio.has_value());
    EXPECT_EQ(portfolio->positions.size(), 3u);
}

TEST_F(EnhancedFakeBrokerTest, MultipleOrders_SameInstrument_AverageCost) {
    broker->setPrice(SBER_FIGI, 100.0);
    broker->placeOrder(ACCOUNT_ID, makeMarketBuy(SBER_FIGI, 10));
    
    broker->setPrice(SBER_FIGI, 200.0);
    broker->placeOrder(ACCOUNT_ID, makeMarketBuy(SBER_FIGI, 10));
    
    auto portfolio = broker->getPortfolio(ACCOUNT_ID);
    ASSERT_EQ(portfolio->positions.size(), 1u);
    
    auto& pos = portfolio->positions[0];
    EXPECT_EQ(pos.quantity, 200);  // 20 лотов * 10 акций = 200 акций
    // Средняя цена должна быть около 150
    EXPECT_NEAR(pos.averagePrice, 150.0, 10.0);
}
