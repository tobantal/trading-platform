/**
 * @file MarketServiceTest.cpp
 * @brief Unit tests for MarketService
 */

#include <gtest/gtest.h>
#include "application/MarketService.hpp"
#include "../mocks/MockBrokerGateway.hpp"

using namespace trading;
using namespace trading::application;
using namespace trading::tests;

class MarketServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockBroker_ = std::make_shared<MockBrokerGateway>();
        marketService_ = std::make_shared<MarketService>(mockBroker_);

        setupTestData();
    }

    void setupTestData() {
        // Котировка SBER
        domain::Quote sberQuote(
            "BBG004730N88",
            "SBER",
            domain::Money::fromDouble(280.0, "RUB"),
            domain::Money::fromDouble(279.5, "RUB"),
            domain::Money::fromDouble(280.5, "RUB")
        );
        mockBroker_->setQuote("BBG004730N88", sberQuote);

        // Инструмент SBER
        domain::Instrument sberInstr("BBG004730N88", "SBER", "Сбербанк", "RUB", 10);
        mockBroker_->setInstrument("BBG004730N88", sberInstr);

        // Инструмент GAZP
        domain::Instrument gazpInstr("BBG004730RP0", "GAZP", "Газпром", "RUB", 10);
        mockBroker_->setInstrument("BBG004730RP0", gazpInstr);
    }

    std::shared_ptr<MockBrokerGateway> mockBroker_;
    std::shared_ptr<MarketService> marketService_;
};

// ============================================================================
// QUOTE TESTS
// ============================================================================

TEST_F(MarketServiceTest, GetQuote_ExistingFigi_ReturnsQuote) {
    auto quote = marketService_->getQuote("BBG004730N88");

    ASSERT_TRUE(quote.has_value());
    EXPECT_EQ(quote->figi, "BBG004730N88");
    EXPECT_EQ(quote->ticker, "SBER");
    EXPECT_NEAR(quote->lastPrice.toDouble(), 280.0, 0.01);
}

TEST_F(MarketServiceTest, GetQuote_UnknownFigi_ReturnsNullopt) {
    auto quote = marketService_->getQuote("UNKNOWN");

    EXPECT_FALSE(quote.has_value());
}

TEST_F(MarketServiceTest, GetQuotes_MultipleIgis_ReturnsQuotes) {
    // Добавляем ещё одну котировку
    domain::Quote gazpQuote(
        "BBG004730RP0",
        "GAZP",
        domain::Money::fromDouble(150.0, "RUB"),
        domain::Money::fromDouble(149.5, "RUB"),
        domain::Money::fromDouble(150.5, "RUB")
    );
    mockBroker_->setQuote("BBG004730RP0", gazpQuote);

    auto quotes = marketService_->getQuotes({"BBG004730N88", "BBG004730RP0"});

    EXPECT_EQ(quotes.size(), 2u);
}

// ============================================================================
// INSTRUMENT TESTS
// ============================================================================

TEST_F(MarketServiceTest, GetInstrumentByFigi_ExistingFigi_ReturnsInstrument) {
    auto instr = marketService_->getInstrumentByFigi("BBG004730N88");

    ASSERT_TRUE(instr.has_value());
    EXPECT_EQ(instr->figi, "BBG004730N88");
    EXPECT_EQ(instr->ticker, "SBER");
    EXPECT_EQ(instr->name, "Сбербанк");
    EXPECT_EQ(instr->lot, 10);
}

TEST_F(MarketServiceTest, GetInstrumentByFigi_UnknownFigi_ReturnsNullopt) {
    auto instr = marketService_->getInstrumentByFigi("UNKNOWN");

    EXPECT_FALSE(instr.has_value());
}

TEST_F(MarketServiceTest, GetAllInstruments_ReturnsAll) {
    auto instruments = marketService_->getAllInstruments();

    EXPECT_EQ(instruments.size(), 2u);
}

TEST_F(MarketServiceTest, SearchInstruments_ByTicker_FindsInstrument) {
    auto instruments = marketService_->searchInstruments("SBER");

    EXPECT_GE(instruments.size(), 1u);
    EXPECT_EQ(instruments[0].ticker, "SBER");
}

TEST_F(MarketServiceTest, SearchInstruments_ByName_FindsInstrument) {
    auto instruments = marketService_->searchInstruments("Газпром");

    EXPECT_GE(instruments.size(), 1u);
    EXPECT_EQ(instruments[0].ticker, "GAZP");
}

TEST_F(MarketServiceTest, SearchInstruments_NoMatch_ReturnsEmpty) {
    auto instruments = marketService_->searchInstruments("NONEXISTENT");

    EXPECT_TRUE(instruments.empty());
}
