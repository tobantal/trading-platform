#include <gtest/gtest.h>
#include "adapters/secondary/broker/FakeTinkoffAdapter.hpp"

using namespace trading::adapters::secondary;
using namespace trading::domain;

class FakeTinkoffAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        adapter = std::make_unique<FakeTinkoffAdapter>();
        adapter->setAccessToken("test-token");
    }

    void TearDown() override {
        adapter->reset();
    }

    std::unique_ptr<FakeTinkoffAdapter> adapter;
    
    const std::string SBER_FIGI = "BBG004730N88";
    const std::string LKOH_FIGI = "BBG004731032";
    const std::string UNKNOWN_FIGI = "UNKNOWN123";
};

// ============================================
// INSTRUMENTS
// ============================================

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

TEST_F(FakeTinkoffAdapterTest, SearchInstruments_ByTicker) {
    auto results = adapter->searchInstruments("sber");
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].ticker, "SBER");
}

TEST_F(FakeTinkoffAdapterTest, SearchInstruments_ByName) {
    auto results = adapter->searchInstruments("Газпром");
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].ticker, "GAZP");
}

TEST_F(FakeTinkoffAdapterTest, SearchInstruments_NoMatch) {
    auto results = adapter->searchInstruments("apple");
    EXPECT_TRUE(results.empty());
}

// ============================================
// QUOTES
// ============================================

TEST_F(FakeTinkoffAdapterTest, GetQuote_ValidFigi) {
    auto quote = adapter->getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    EXPECT_EQ(quote->figi, SBER_FIGI);
    EXPECT_EQ(quote->ticker, "SBER");
    
    // Цена в диапазоне ±5% от 265
    double price = quote->lastPrice.toDouble();
    EXPECT_GE(price, 265.0 * 0.95);
    EXPECT_LE(price, 265.0 * 1.05);
}

TEST_F(FakeTinkoffAdapterTest, GetQuote_InvalidFigi) {
    auto quote = adapter->getQuote(UNKNOWN_FIGI);
    EXPECT_FALSE(quote.has_value());
}

TEST_F(FakeTinkoffAdapterTest, GetQuotes_Multiple) {
    auto quotes = adapter->getQuotes({SBER_FIGI, LKOH_FIGI, UNKNOWN_FIGI});
    EXPECT_EQ(quotes.size(), 2);  // UNKNOWN не вернётся
}

TEST_F(FakeTinkoffAdapterTest, GetQuote_HasSpread) {
    auto quote = adapter->getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    EXPECT_LT(quote->bidPrice.toDouble(), quote->lastPrice.toDouble());
    EXPECT_GT(quote->askPrice.toDouble(), quote->lastPrice.toDouble());
}

// ============================================
// PORTFOLIO
// ============================================

TEST_F(FakeTinkoffAdapterTest, InitialCash_OneMillion) {
    auto cash = adapter->getCash();
    EXPECT_DOUBLE_EQ(cash.toDouble(), 1000000.0);
}

TEST_F(FakeTinkoffAdapterTest, InitialPositions_Empty) {
    auto positions = adapter->getPositions();
    EXPECT_TRUE(positions.empty());
}

TEST_F(FakeTinkoffAdapterTest, GetPortfolio_Initial) {
    auto portfolio = adapter->getPortfolio();
    EXPECT_DOUBLE_EQ(portfolio.cash.toDouble(), 1000000.0);
    EXPECT_TRUE(portfolio.positions.empty());
}

// ============================================
// ORDERS - BUY
// ============================================

TEST_F(FakeTinkoffAdapterTest, PlaceOrder_MarketBuy_Success) {
    OrderRequest req;
    req.accountId = "acc-1";
    req.figi = SBER_FIGI;
    req.direction = OrderDirection::BUY;
    req.type = OrderType::MARKET;
    req.quantity = 10;  // 10 лотов = 100 акций
    
    auto result = adapter->placeOrder(req);
    
    EXPECT_EQ(result.status, OrderStatus::FILLED);
    EXPECT_EQ(result.message, "OK");
    EXPECT_FALSE(result.orderId.empty());
    
    // Проверяем позицию
    auto positions = adapter->getPositions();
    ASSERT_EQ(positions.size(), 1);
    EXPECT_EQ(positions[0].figi, SBER_FIGI);
    EXPECT_EQ(positions[0].quantity, 10);
    
    // Проверяем уменьшение кэша
    auto cash = adapter->getCash();
    EXPECT_LT(cash.toDouble(), 1000000.0);
}

TEST_F(FakeTinkoffAdapterTest, PlaceOrder_MarketBuy_InsufficientFunds) {
    adapter->setCash(Money::fromDouble(100.0, "RUB"));  // Мало денег
    
    OrderRequest req;
    req.accountId = "acc-1";
    req.figi = LKOH_FIGI;  // ~7200 руб за акцию
    req.direction = OrderDirection::BUY;
    req.type = OrderType::MARKET;
    req.quantity = 1;
    
    auto result = adapter->placeOrder(req);
    
    EXPECT_EQ(result.status, OrderStatus::REJECTED);
    EXPECT_EQ(result.message, "Insufficient funds");
}

TEST_F(FakeTinkoffAdapterTest, PlaceOrder_UnknownInstrument) {
    OrderRequest req;
    req.accountId = "acc-1";
    req.figi = UNKNOWN_FIGI;
    req.direction = OrderDirection::BUY;
    req.type = OrderType::MARKET;
    req.quantity = 1;
    
    auto result = adapter->placeOrder(req);
    
    EXPECT_EQ(result.status, OrderStatus::REJECTED);
    EXPECT_TRUE(result.message.find("Unknown instrument") != std::string::npos);
}

TEST_F(FakeTinkoffAdapterTest, PlaceOrder_LimitBuy_Pending) {
    OrderRequest req;
    req.accountId = "acc-1";
    req.figi = SBER_FIGI;
    req.direction = OrderDirection::BUY;
    req.type = OrderType::LIMIT;
    req.quantity = 10;
    req.price = Money::fromDouble(260.0, "RUB");
    
    auto result = adapter->placeOrder(req);
    
    EXPECT_EQ(result.status, OrderStatus::PENDING);
    
    // Кэш не должен измениться
    EXPECT_DOUBLE_EQ(adapter->getCash().toDouble(), 1000000.0);
}

// ============================================
// ORDERS - SELL
// ============================================

TEST_F(FakeTinkoffAdapterTest, PlaceOrder_MarketSell_Success) {
    // Сначала покупаем
    OrderRequest buyReq;
    buyReq.accountId = "acc-1";
    buyReq.figi = SBER_FIGI;
    buyReq.direction = OrderDirection::BUY;
    buyReq.type = OrderType::MARKET;
    buyReq.quantity = 10;
    adapter->placeOrder(buyReq);
    
    double cashAfterBuy = adapter->getCash().toDouble();
    
    // Продаём часть
    OrderRequest sellReq;
    sellReq.accountId = "acc-1";
    sellReq.figi = SBER_FIGI;
    sellReq.direction = OrderDirection::SELL;
    sellReq.type = OrderType::MARKET;
    sellReq.quantity = 5;
    
    auto result = adapter->placeOrder(sellReq);
    
    EXPECT_EQ(result.status, OrderStatus::FILLED);
    
    // Проверяем позицию уменьшилась
    auto positions = adapter->getPositions();
    ASSERT_EQ(positions.size(), 1);
    EXPECT_EQ(positions[0].quantity, 5);
    
    // Кэш увеличился
    EXPECT_GT(adapter->getCash().toDouble(), cashAfterBuy);
}

TEST_F(FakeTinkoffAdapterTest, PlaceOrder_MarketSell_InsufficientPosition) {
    OrderRequest req;
    req.accountId = "acc-1";
    req.figi = SBER_FIGI;
    req.direction = OrderDirection::SELL;
    req.type = OrderType::MARKET;
    req.quantity = 10;
    
    auto result = adapter->placeOrder(req);
    
    EXPECT_EQ(result.status, OrderStatus::REJECTED);
    EXPECT_EQ(result.message, "Insufficient position");
}

TEST_F(FakeTinkoffAdapterTest, PlaceOrder_SellAll_PositionRemoved) {
    // Покупаем
    OrderRequest buyReq;
    buyReq.accountId = "acc-1";
    buyReq.figi = SBER_FIGI;
    buyReq.direction = OrderDirection::BUY;
    buyReq.type = OrderType::MARKET;
    buyReq.quantity = 10;
    adapter->placeOrder(buyReq);
    
    // Продаём всё
    OrderRequest sellReq;
    sellReq.accountId = "acc-1";
    sellReq.figi = SBER_FIGI;
    sellReq.direction = OrderDirection::SELL;
    sellReq.type = OrderType::MARKET;
    sellReq.quantity = 10;
    adapter->placeOrder(sellReq);
    
    // Позиция должна исчезнуть
    auto positions = adapter->getPositions();
    EXPECT_TRUE(positions.empty());
}

// ============================================
// ORDERS - CANCEL
// ============================================

TEST_F(FakeTinkoffAdapterTest, CancelOrder_Pending_Success) {
    OrderRequest req;
    req.accountId = "acc-1";
    req.figi = SBER_FIGI;
    req.direction = OrderDirection::BUY;
    req.type = OrderType::LIMIT;
    req.quantity = 10;
    req.price = Money::fromDouble(260.0, "RUB");
    
    auto result = adapter->placeOrder(req);
    ASSERT_EQ(result.status, OrderStatus::PENDING);
    
    bool cancelled = adapter->cancelOrder(result.orderId);
    EXPECT_TRUE(cancelled);
    
    auto order = adapter->getOrderStatus(result.orderId);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->status, OrderStatus::CANCELLED);
}

TEST_F(FakeTinkoffAdapterTest, CancelOrder_Filled_Fails) {
    OrderRequest req;
    req.accountId = "acc-1";
    req.figi = SBER_FIGI;
    req.direction = OrderDirection::BUY;
    req.type = OrderType::MARKET;
    req.quantity = 10;
    
    auto result = adapter->placeOrder(req);
    ASSERT_EQ(result.status, OrderStatus::FILLED);
    
    bool cancelled = adapter->cancelOrder(result.orderId);
    EXPECT_FALSE(cancelled);
}

TEST_F(FakeTinkoffAdapterTest, CancelOrder_NotFound_Fails) {
    bool cancelled = adapter->cancelOrder("non-existent-id");
    EXPECT_FALSE(cancelled);
}

// ============================================
// ORDERS - STATUS & LIST
// ============================================

TEST_F(FakeTinkoffAdapterTest, GetOrderStatus_Found) {
    OrderRequest req;
    req.accountId = "acc-1";
    req.figi = SBER_FIGI;
    req.direction = OrderDirection::BUY;
    req.type = OrderType::MARKET;
    req.quantity = 10;
    
    auto result = adapter->placeOrder(req);
    auto order = adapter->getOrderStatus(result.orderId);
    
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->id, result.orderId);
    EXPECT_EQ(order->figi, SBER_FIGI);
}

TEST_F(FakeTinkoffAdapterTest, GetOrderStatus_NotFound) {
    auto order = adapter->getOrderStatus("non-existent");
    EXPECT_FALSE(order.has_value());
}

TEST_F(FakeTinkoffAdapterTest, GetOrders_ReturnsAll) {
    // Создаём 3 ордера
    for (int i = 0; i < 3; i++) {
        OrderRequest req;
        req.accountId = "acc-1";
        req.figi = SBER_FIGI;
        req.direction = OrderDirection::BUY;
        req.type = OrderType::MARKET;
        req.quantity = 1;
        adapter->placeOrder(req);
    }
    
    auto orders = adapter->getOrders();
    EXPECT_EQ(orders.size(), 3);
}

// ============================================
// POSITION AVERAGING
// ============================================

TEST_F(FakeTinkoffAdapterTest, MultipleBuys_AveragesPosition) {
    // Первая покупка
    OrderRequest req1;
    req1.accountId = "acc-1";
    req1.figi = SBER_FIGI;
    req1.direction = OrderDirection::BUY;
    req1.type = OrderType::MARKET;
    req1.quantity = 10;
    adapter->placeOrder(req1);
    
    auto pos1 = adapter->getPositions()[0];
    double avgPrice1 = pos1.averagePrice.toDouble();
    
    // Вторая покупка
    OrderRequest req2;
    req2.accountId = "acc-1";
    req2.figi = SBER_FIGI;
    req2.direction = OrderDirection::BUY;
    req2.type = OrderType::MARKET;
    req2.quantity = 10;
    adapter->placeOrder(req2);
    
    auto positions = adapter->getPositions();
    ASSERT_EQ(positions.size(), 1);
    EXPECT_EQ(positions[0].quantity, 20);
    
    // Средняя цена должна быть между двумя покупками
    // (не проверяем точно, т.к. цены случайные)
}