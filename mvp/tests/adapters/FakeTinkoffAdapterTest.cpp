#include <gtest/gtest.h>
#include "adapters/secondary/broker/FakeTinkoffAdapter.hpp"

using namespace trading::adapters::secondary;
using namespace trading::domain;

class FakeTinkoffAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        adapter = std::make_unique<FakeTinkoffAdapter>();
        adapter->registerAccount(TEST_ACCOUNT_ID, "test-token");
    }

    void TearDown() override {
        adapter->reset();
    }

    std::unique_ptr<FakeTinkoffAdapter> adapter;
    
    const std::string TEST_ACCOUNT_ID = "test-account";
    const std::string SBER_FIGI = "BBG004730N88";
    const std::string LKOH_FIGI = "BBG004731032";
    const std::string UNKNOWN_FIGI = "UNKNOWN123";
};

// Инструменты
TEST_F(FakeTinkoffAdapterTest, GetAllInstruments_ReturnsFive) {
    auto instruments = adapter->getAllInstruments();
    EXPECT_EQ(instruments.size(), 5);
}

TEST_F(FakeTinkoffAdapterTest, GetInstrumentByFigi_Found) {
    auto instr = adapter->getInstrumentByFigi(SBER_FIGI);
    ASSERT_TRUE(instr.has_value());
    EXPECT_EQ(instr->ticker, "SBER");
    EXPECT_EQ(instr->lot, 10);
}

TEST_F(FakeTinkoffAdapterTest, GetInstrumentByFigi_NotFound) {
    auto instr = adapter->getInstrumentByFigi(UNKNOWN_FIGI);
    EXPECT_FALSE(instr.has_value());
}

// Котировки
TEST_F(FakeTinkoffAdapterTest, GetQuote_ValidFigi) {
    auto quote = adapter->getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    EXPECT_EQ(quote->figi, SBER_FIGI);
    EXPECT_EQ(quote->ticker, "SBER");
}

TEST_F(FakeTinkoffAdapterTest, GetQuote_InvalidFigi) {
    auto quote = adapter->getQuote(UNKNOWN_FIGI);
    EXPECT_FALSE(quote.has_value());
}

// Портфель
TEST_F(FakeTinkoffAdapterTest, GetPortfolio_Initial) {
    auto portfolio = adapter->getPortfolio(TEST_ACCOUNT_ID);
    EXPECT_DOUBLE_EQ(portfolio.cash.toDouble(), 1000000.0);
    EXPECT_TRUE(portfolio.positions.empty());
}

TEST_F(FakeTinkoffAdapterTest, GetPortfolio_AccountNotFound) {
    EXPECT_THROW(adapter->getPortfolio("non-existent"), std::runtime_error);
}

// Ордера
TEST_F(FakeTinkoffAdapterTest, PlaceOrder_MarketBuy_Success) {
    OrderRequest req;
    req.accountId = TEST_ACCOUNT_ID;
    req.figi = SBER_FIGI;
    req.direction = OrderDirection::BUY;
    req.type = OrderType::MARKET;
    req.quantity = 10;
    
    auto result = adapter->placeOrder(TEST_ACCOUNT_ID, req);
    EXPECT_EQ(result.status, OrderStatus::FILLED);
    EXPECT_FALSE(result.orderId.empty());
}

TEST_F(FakeTinkoffAdapterTest, PlaceOrder_MarketBuy_InsufficientFunds) {
    adapter->setCash(TEST_ACCOUNT_ID, Money::fromDouble(100.0, "RUB"));
    
    OrderRequest req;
    req.accountId = TEST_ACCOUNT_ID;
    req.figi = LKOH_FIGI;
    req.direction = OrderDirection::BUY;
    req.type = OrderType::MARKET;
    req.quantity = 1;
    
    auto result = adapter->placeOrder(TEST_ACCOUNT_ID, req);
    EXPECT_EQ(result.status, OrderStatus::REJECTED);
}

TEST_F(FakeTinkoffAdapterTest, PlaceOrder_LimitBuy_Pending) {
    OrderRequest req;
    req.accountId = TEST_ACCOUNT_ID;
    req.figi = SBER_FIGI;
    req.direction = OrderDirection::BUY;
    req.type = OrderType::LIMIT;
    req.quantity = 10;
    req.price = Money::fromDouble(260.0, "RUB");
    
    auto result = adapter->placeOrder(TEST_ACCOUNT_ID, req);
    EXPECT_EQ(result.status, OrderStatus::PENDING);
}

TEST_F(FakeTinkoffAdapterTest, PlaceOrder_AccountNotFound) {
    OrderRequest req;
    req.accountId = "non-existent";
    req.figi = SBER_FIGI;
    req.direction = OrderDirection::BUY;
    req.type = OrderType::MARKET;
    req.quantity = 10;
    
    auto result = adapter->placeOrder("non-existent", req);
    EXPECT_EQ(result.status, OrderStatus::REJECTED);
}

// Отмена ордеров
TEST_F(FakeTinkoffAdapterTest, CancelOrder_Pending_Success) {
    OrderRequest req;
    req.accountId = TEST_ACCOUNT_ID;
    req.figi = SBER_FIGI;
    req.direction = OrderDirection::BUY;
    req.type = OrderType::LIMIT;
    req.quantity = 10;
    req.price = Money::fromDouble(260.0, "RUB");
    
    auto result = adapter->placeOrder(TEST_ACCOUNT_ID, req);
    bool cancelled = adapter->cancelOrder(TEST_ACCOUNT_ID, result.orderId);
    EXPECT_TRUE(cancelled);
}

TEST_F(FakeTinkoffAdapterTest, CancelOrder_Filled_Fails) {
    OrderRequest req;
    req.accountId = TEST_ACCOUNT_ID;
    req.figi = SBER_FIGI;
    req.direction = OrderDirection::BUY;
    req.type = OrderType::MARKET;
    req.quantity = 10;
    
    auto result = adapter->placeOrder(TEST_ACCOUNT_ID, req);
    bool cancelled = adapter->cancelOrder(TEST_ACCOUNT_ID, result.orderId);
    EXPECT_FALSE(cancelled);
}

// Получение ордеров
TEST_F(FakeTinkoffAdapterTest, GetOrderStatus_Found) {
    OrderRequest req;
    req.accountId = TEST_ACCOUNT_ID;
    req.figi = SBER_FIGI;
    req.direction = OrderDirection::BUY;
    req.type = OrderType::MARKET;
    req.quantity = 10;
    
    auto result = adapter->placeOrder(TEST_ACCOUNT_ID, req);
    auto order = adapter->getOrderStatus(TEST_ACCOUNT_ID, result.orderId);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->id, result.orderId);
}

TEST_F(FakeTinkoffAdapterTest, GetOrders_ReturnsAll) {
    for (int i = 0; i < 3; i++) {
        OrderRequest req;
        req.accountId = TEST_ACCOUNT_ID;
        req.figi = SBER_FIGI;
        req.direction = OrderDirection::BUY;
        req.type = OrderType::MARKET;
        req.quantity = 1;
        adapter->placeOrder(TEST_ACCOUNT_ID, req);
    }
    
    auto orders = adapter->getOrders(TEST_ACCOUNT_ID);
    EXPECT_EQ(orders.size(), 3);
}

// Несколько аккаунтов
TEST_F(FakeTinkoffAdapterTest, MultipleAccounts_Isolated) {
    const std::string ACCOUNT1 = "account-1";
    const std::string ACCOUNT2 = "account-2";
    
    adapter->registerAccount(ACCOUNT1, "token-1");
    adapter->registerAccount(ACCOUNT2, "token-2");
    
    adapter->setCash(ACCOUNT1, Money::fromDouble(500000.0, "RUB"));
    adapter->setCash(ACCOUNT2, Money::fromDouble(2000000.0, "RUB"));
    
    OrderRequest req;
    req.accountId = ACCOUNT1;
    req.figi = SBER_FIGI;
    req.direction = OrderDirection::BUY;
    req.type = OrderType::MARKET;
    req.quantity = 5;
    adapter->placeOrder(ACCOUNT1, req);
    
    auto portfolio1 = adapter->getPortfolio(ACCOUNT1);
    auto portfolio2 = adapter->getPortfolio(ACCOUNT2);
    
    EXPECT_EQ(portfolio1.positions.size(), 1);
    EXPECT_TRUE(portfolio2.positions.empty());
}