/**
 * @file EnhancedFakeBrokerTest.cpp
 * @brief Unit tests for EnhancedFakeBroker
 */

#include <gtest/gtest.h>
#include "adapters/secondary/broker/EnhancedFakeBroker.hpp"
#include "settings/BrokerSettings.hpp"

using namespace broker::adapters::secondary;
using namespace broker::settings;

class EnhancedFakeBrokerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Создаём settings для тестов
        settings_ = std::make_shared<BrokerSettings>();
        
        // Создаём брокера с settings
        broker_ = std::make_unique<EnhancedFakeBroker>(settings_);
        broker_->registerAccount(TEST_ACCOUNT, "test-token");
    }

    void TearDown() override {
        broker_.reset();
    }
    
    BrokerOrderRequest createBuyMarket(const std::string& figi, int64_t qty) {
        BrokerOrderRequest req;
        req.accountId = TEST_ACCOUNT;
        req.figi = figi;
        req.direction = Direction::BUY;
        req.type = Type::MARKET;
        req.quantity = qty;
        return req;
    }
    
    BrokerOrderRequest createSellMarket(const std::string& figi, int64_t qty) {
        BrokerOrderRequest req;
        req.accountId = TEST_ACCOUNT;
        req.figi = figi;
        req.direction = Direction::SELL;
        req.type = Type::MARKET;
        req.quantity = qty;
        return req;
    }
    
    BrokerOrderRequest createBuyLimit(const std::string& figi, int64_t qty, double price) {
        BrokerOrderRequest req;
        req.accountId = TEST_ACCOUNT;
        req.figi = figi;
        req.direction = Direction::BUY;
        req.type = Type::LIMIT;
        req.quantity = qty;
        req.price = price;
        return req;
    }

    std::shared_ptr<BrokerSettings> settings_;
    std::unique_ptr<EnhancedFakeBroker> broker_;
    const std::string TEST_ACCOUNT = "test-account";
    const std::string SBER_FIGI = "BBG004730N88";
    const std::string LKOH_FIGI = "BBG004731032";
};


// ============================================================================
// BASIC OPERATIONS
// ============================================================================

TEST_F(EnhancedFakeBrokerTest, GetQuote_ReturnsValidQuote) {
    auto quote = broker_->getQuote(SBER_FIGI);
    
    ASSERT_TRUE(quote.has_value());
    EXPECT_GT(quote->bidPrice, 0.0);
    EXPECT_GT(quote->askPrice, 0.0);
    EXPECT_GT(quote->lastPrice, 0.0);
}

TEST_F(EnhancedFakeBrokerTest, GetQuote_UnknownFigi_ReturnsEmpty) {
    auto quote = broker_->getQuote("UNKNOWN");
    EXPECT_FALSE(quote.has_value());
}

TEST_F(EnhancedFakeBrokerTest, PlaceOrder_BuyMarket_Fills) {
    auto req = createBuyMarket(SBER_FIGI, 10);
    auto result = broker_->placeOrder(TEST_ACCOUNT, req);
    
    EXPECT_TRUE(result.isSuccess());
    EXPECT_EQ(result.status, Status::FILLED);
    EXPECT_EQ(result.executedQuantity, 10);
}

TEST_F(EnhancedFakeBrokerTest, PlaceOrder_SellMarket_WithoutPosition_Rejected) {
    auto req = createSellMarket(SBER_FIGI, 5);
    auto result = broker_->placeOrder(TEST_ACCOUNT, req);
    
    EXPECT_EQ(result.status, Status::REJECTED);
}

TEST_F(EnhancedFakeBrokerTest, PlaceOrder_SellMarket_WithPosition_Fills) {
    // First buy
    auto buyReq = createBuyMarket(SBER_FIGI, 10);
    broker_->placeOrder(TEST_ACCOUNT, buyReq);
    
    // Then sell
    auto sellReq = createSellMarket(SBER_FIGI, 5);
    auto result = broker_->placeOrder(TEST_ACCOUNT, sellReq);
    
    EXPECT_TRUE(result.isSuccess());
    EXPECT_EQ(result.executedQuantity, 5);
}

TEST_F(EnhancedFakeBrokerTest, PlaceOrder_LimitBuy_Queued) {
    auto req = createBuyLimit(SBER_FIGI, 10, 200.0);  // Low price
    auto result = broker_->placeOrder(TEST_ACCOUNT, req);
    
    EXPECT_EQ(result.status, Status::PENDING);
    EXPECT_GT(broker_->pendingOrderCount(), 0u);
}


// ============================================================================
// PORTFOLIO MANAGEMENT
// ============================================================================

TEST_F(EnhancedFakeBrokerTest, GetPortfolio_ReturnsValidPortfolio) {
    auto portfolio = broker_->getPortfolio(TEST_ACCOUNT);
    
    EXPECT_EQ(portfolio.accountId, TEST_ACCOUNT);
    EXPECT_GT(portfolio.cash, 0.0);
}

TEST_F(EnhancedFakeBrokerTest, SetCash_ChangesCash) {
    broker_->setCash(TEST_ACCOUNT, 500000.0);
    auto portfolio = broker_->getPortfolio(TEST_ACCOUNT);
    
    EXPECT_DOUBLE_EQ(portfolio.cash, 500000.0);
}

TEST_F(EnhancedFakeBrokerTest, BuyOrder_ReducesCash) {
    auto portfolioBefore = broker_->getPortfolio(TEST_ACCOUNT);
    double initialCash = portfolioBefore.cash;
    
    auto req = createBuyMarket(SBER_FIGI, 10);
    broker_->placeOrder(TEST_ACCOUNT, req);
    
    auto portfolioAfter = broker_->getPortfolio(TEST_ACCOUNT);
    EXPECT_LT(portfolioAfter.cash, initialCash);
}

TEST_F(EnhancedFakeBrokerTest, GetPortfolio_AfterBuy_HasPosition) {
    auto req = createBuyMarket(SBER_FIGI, 10);
    broker_->placeOrder(TEST_ACCOUNT, req);
    
    auto portfolio = broker_->getPortfolio(TEST_ACCOUNT);
    EXPECT_FALSE(portfolio.positions.empty());
}


// ============================================================================
// SCENARIO CONFIGURATION
// ============================================================================

TEST_F(EnhancedFakeBrokerTest, SetScenario_PartialFill) {
    auto scenario = MarketScenario::partialFill(280.0, 0.5);
    broker_->setScenario(SBER_FIGI, scenario);
    
    auto req = createBuyMarket(SBER_FIGI, 10);
    auto result = broker_->placeOrder(TEST_ACCOUNT, req);
    
    EXPECT_EQ(result.status, Status::PARTIALLY_FILLED);
    EXPECT_EQ(result.executedQuantity, 5);
}

TEST_F(EnhancedFakeBrokerTest, SetScenario_AlwaysReject) {
    auto scenario = MarketScenario::alwaysReject("Market closed");
    broker_->setScenario(SBER_FIGI, scenario);
    
    auto req = createBuyMarket(SBER_FIGI, 10);
    auto result = broker_->placeOrder(TEST_ACCOUNT, req);
    
    EXPECT_EQ(result.status, Status::REJECTED);
    EXPECT_EQ(result.message, "Market closed");
}


// ============================================================================
// ORDER MANAGEMENT
// ============================================================================

// TODO: нестабильный тест, зависит от настроек брокера
// сейчас у LKOH стоит 10 секунд ожидания (pending), поэтому тест должен проходить
// в будущем передавать расширенные настройки, т.е. вначале для бумаги установить pending 3s,
// потом запускать проверку отмены.
TEST_F(EnhancedFakeBrokerTest, CancelOrder_PendingLimit) {
    auto req = createBuyLimit(LKOH_FIGI, 10, 200.0);
    auto result = broker_->placeOrder(TEST_ACCOUNT, req);
    EXPECT_EQ(result.status, Status::PENDING);
    
    bool cancelled = broker_->cancelOrder(TEST_ACCOUNT, result.orderId);
    EXPECT_TRUE(cancelled);
    EXPECT_EQ(broker_->pendingOrderCount(), 0u);
}


// ============================================================================
// SIMULATION CONTROL
// ============================================================================

TEST_F(EnhancedFakeBrokerTest, StartStopSimulation) {
    EXPECT_FALSE(broker_->isSimulationRunning());
    
    broker_->startSimulation();
    EXPECT_TRUE(broker_->isSimulationRunning());
    
    broker_->stopSimulation();
    EXPECT_FALSE(broker_->isSimulationRunning());
}


// ============================================================================
// INSTRUMENT ACCESS
// ============================================================================

TEST_F(EnhancedFakeBrokerTest, GetInstrument_ReturnsInstrument) {
    auto instr = broker_->getInstrument(SBER_FIGI);
    
    ASSERT_TRUE(instr.has_value());
    EXPECT_EQ(instr->ticker, "SBER");
}

TEST_F(EnhancedFakeBrokerTest, GetAllInstruments_ReturnsMultiple) {
    auto instruments = broker_->getAllInstruments();
    EXPECT_GT(instruments.size(), 0u);
}


// ============================================================================
// RESET
// ============================================================================

/* TODO: нестабильны тест, потом разобраться
TEST_F(EnhancedFakeBrokerTest, Reset_ClearsAllData) {
    auto req = createBuyMarket(SBER_FIGI, 10);
    broker_->placeOrder(TEST_ACCOUNT, req);
    
    broker_->reset();
    
    EXPECT_FALSE(broker_->hasAccount(TEST_ACCOUNT));
}
*/


// ============================================================================
// SETTINGS INTEGRATION
// ============================================================================

TEST_F(EnhancedFakeBrokerTest, UsesSettingsFromDI) {
    // Проверяем что брокер создался с настройками
    EXPECT_NE(settings_, nullptr);
    
    // Брокер должен работать нормально
    auto quote = broker_->getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
}
