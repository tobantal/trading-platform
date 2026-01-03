#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/secondary/CachedBrokerGateway.hpp"
#include "settings/CacheSettings.hpp"
#include "settings/IBrokerClientSettings.hpp"
#include <IHttpClient.hpp>

using namespace trading;
using namespace trading::adapters::secondary;
using ::testing::Return;
using ::testing::_;

// ============================================================================
// Mocks
// ============================================================================

class MockBrokerClientSettings : public settings::IBrokerClientSettings {
public:
    std::string getHost() const override { return "mock-host"; }
    int getPort() const override { return 9999; }
};

class MockHttpClient : public IHttpClient {
public:
    MOCK_METHOD(bool, send, (const IRequest& req, IResponse& res), (override));
};

class MockHttpBrokerGateway : public HttpBrokerGateway {
public:
    MockHttpBrokerGateway() 
        : HttpBrokerGateway(
            std::make_shared<MockHttpClient>(),
            std::make_shared<MockBrokerClientSettings>()
        ) {}

    MOCK_METHOD(std::optional<domain::Quote>, getQuote, (const std::string& figi), (override));
    MOCK_METHOD(std::vector<domain::Quote>, getQuotes, (const std::vector<std::string>& figis), (override));
    MOCK_METHOD(std::optional<domain::Instrument>, getInstrumentByFigi, (const std::string& figi), (override));
    MOCK_METHOD(std::vector<domain::Instrument>, getAllInstruments, (), (override));
    MOCK_METHOD(std::vector<domain::Instrument>, searchInstruments, (const std::string& query), (override));
    MOCK_METHOD(domain::Portfolio, getPortfolio, (const std::string& accountId), (override));
    MOCK_METHOD(std::vector<domain::Order>, getOrders, (const std::string& accountId), (override));
    MOCK_METHOD(std::optional<domain::Order>, getOrder, (const std::string& accountId, const std::string& orderId), (override));
};

// ============================================================================
// Test Fixture
// ============================================================================

class CachedBrokerGatewayTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockDelegate_ = std::make_shared<MockHttpBrokerGateway>();
        cacheSettings_ = std::make_shared<settings::CacheSettings>();
        
        cachedGateway_ = std::make_shared<CachedBrokerGateway>(
            mockDelegate_,
            cacheSettings_
        );
    }

    domain::Quote createQuote(const std::string& figi, const std::string& ticker, double price) {
        return domain::Quote(
            figi, ticker,
            domain::Money::fromDouble(price, "RUB"),
            domain::Money::fromDouble(price - 0.5, "RUB"),
            domain::Money::fromDouble(price + 0.5, "RUB")
        );
    }

    domain::Instrument createInstrument(const std::string& figi, const std::string& ticker, 
                                         const std::string& name) {
        return domain::Instrument(figi, ticker, name, "RUB", 10);
    }

    std::shared_ptr<MockHttpBrokerGateway> mockDelegate_;
    std::shared_ptr<settings::CacheSettings> cacheSettings_;
    std::shared_ptr<CachedBrokerGateway> cachedGateway_;
};

// ============================================================================
// ТЕСТЫ: getQuote
// ============================================================================

TEST_F(CachedBrokerGatewayTest, GetQuote_FirstCall_DelegatesToHttpGateway) {
    auto sberQuote = createQuote("BBG004730N88", "SBER", 280.0);
    
    EXPECT_CALL(*mockDelegate_, getQuote("BBG004730N88"))
        .Times(1)
        .WillOnce(Return(sberQuote));

    auto result = cachedGateway_->getQuote("BBG004730N88");
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->figi, "BBG004730N88");
}

TEST_F(CachedBrokerGatewayTest, GetQuote_SecondCall_UsesCache) {
    auto sberQuote = createQuote("BBG004730N88", "SBER", 280.0);
    
    EXPECT_CALL(*mockDelegate_, getQuote("BBG004730N88"))
        .Times(1)
        .WillOnce(Return(sberQuote));

    cachedGateway_->getQuote("BBG004730N88");
    auto result = cachedGateway_->getQuote("BBG004730N88");
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ticker, "SBER");
}

TEST_F(CachedBrokerGatewayTest, GetQuote_NotFound_ReturnsNullopt) {
    EXPECT_CALL(*mockDelegate_, getQuote("UNKNOWN"))
        .Times(1)
        .WillOnce(Return(std::nullopt));

    auto result = cachedGateway_->getQuote("UNKNOWN");
    
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// ТЕСТЫ: getQuotes
// ============================================================================

TEST_F(CachedBrokerGatewayTest, GetQuotes_PartialCache_OnlyFetchesMissing) {
    auto sberQuote = createQuote("BBG004730N88", "SBER", 280.0);
    auto gazpQuote = createQuote("BBG004730RP0", "GAZP", 150.0);
    
    EXPECT_CALL(*mockDelegate_, getQuote("BBG004730N88"))
        .Times(1)
        .WillOnce(Return(sberQuote));
    cachedGateway_->getQuote("BBG004730N88");
    
    EXPECT_CALL(*mockDelegate_, getQuotes(std::vector<std::string>{"BBG004730RP0"}))
        .Times(1)
        .WillOnce(Return(std::vector<domain::Quote>{gazpQuote}));

    auto result = cachedGateway_->getQuotes({"BBG004730N88", "BBG004730RP0"});
    
    ASSERT_EQ(result.size(), 2);
}

// ============================================================================
// ТЕСТЫ: getInstrumentByFigi
// ============================================================================

TEST_F(CachedBrokerGatewayTest, GetInstrumentByFigi_FirstCall_DelegatesToHttpGateway) {
    auto sber = createInstrument("BBG004730N88", "SBER", "Сбербанк");
    
    EXPECT_CALL(*mockDelegate_, getInstrumentByFigi("BBG004730N88"))
        .Times(1)
        .WillOnce(Return(sber));

    auto result = cachedGateway_->getInstrumentByFigi("BBG004730N88");
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ticker, "SBER");
}

TEST_F(CachedBrokerGatewayTest, GetInstrumentByFigi_SecondCall_UsesCache) {
    auto sber = createInstrument("BBG004730N88", "SBER", "Сбербанк");
    
    EXPECT_CALL(*mockDelegate_, getInstrumentByFigi("BBG004730N88"))
        .Times(1)
        .WillOnce(Return(sber));

    cachedGateway_->getInstrumentByFigi("BBG004730N88");
    auto result = cachedGateway_->getInstrumentByFigi("BBG004730N88");
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ticker, "SBER");
}

// ============================================================================
// ТЕСТЫ: getAllInstruments
// ============================================================================

TEST_F(CachedBrokerGatewayTest, GetAllInstruments_AlwaysDelegates) {
    std::vector<domain::Instrument> instruments = {
        createInstrument("BBG004730N88", "SBER", "Сбербанк"),
        createInstrument("BBG004730RP0", "GAZP", "Газпром")
    };
    
    EXPECT_CALL(*mockDelegate_, getAllInstruments())
        .Times(2)
        .WillRepeatedly(Return(instruments));

    cachedGateway_->getAllInstruments();
    cachedGateway_->getAllInstruments();
}

TEST_F(CachedBrokerGatewayTest, GetAllInstruments_WarmsCache_ForGetInstrumentByFigi) {
    std::vector<domain::Instrument> instruments = {
        createInstrument("BBG004730N88", "SBER", "Сбербанк"),
        createInstrument("BBG004730RP0", "GAZP", "Газпром")
    };
    
    EXPECT_CALL(*mockDelegate_, getAllInstruments())
        .Times(1)
        .WillOnce(Return(instruments));
    
    EXPECT_CALL(*mockDelegate_, getInstrumentByFigi(_))
        .Times(0);

    cachedGateway_->getAllInstruments();
    
    auto sber = cachedGateway_->getInstrumentByFigi("BBG004730N88");
    auto gazp = cachedGateway_->getInstrumentByFigi("BBG004730RP0");
    
    ASSERT_TRUE(sber.has_value());
    EXPECT_EQ(sber->ticker, "SBER");
    
    ASSERT_TRUE(gazp.has_value());
    EXPECT_EQ(gazp->ticker, "GAZP");
}

// ============================================================================
// ТЕСТЫ: searchInstruments
// ============================================================================

TEST_F(CachedBrokerGatewayTest, SearchInstruments_AlwaysDelegates) {
    std::vector<domain::Instrument> results = {
        createInstrument("BBG004730N88", "SBER", "Сбербанк")
    };
    
    EXPECT_CALL(*mockDelegate_, searchInstruments("SBER"))
        .Times(2)
        .WillRepeatedly(Return(results));

    cachedGateway_->searchInstruments("SBER");
    cachedGateway_->searchInstruments("SBER");
}

TEST_F(CachedBrokerGatewayTest, SearchInstruments_WarmsCache_ForGetInstrumentByFigi) {
    std::vector<domain::Instrument> results = {
        createInstrument("BBG004730N88", "SBER", "Сбербанк")
    };
    
    EXPECT_CALL(*mockDelegate_, searchInstruments("SBER"))
        .Times(1)
        .WillOnce(Return(results));
    
    EXPECT_CALL(*mockDelegate_, getInstrumentByFigi(_))
        .Times(0);

    cachedGateway_->searchInstruments("SBER");
    
    auto sber = cachedGateway_->getInstrumentByFigi("BBG004730N88");
    
    ASSERT_TRUE(sber.has_value());
    EXPECT_EQ(sber->ticker, "SBER");
}

// ============================================================================
// ТЕСТЫ: Portfolio/Orders НЕ кэшируются
// ============================================================================

TEST_F(CachedBrokerGatewayTest, GetPortfolio_NeverCaches) {
    domain::Portfolio portfolio;
    portfolio.cash = domain::Money::fromDouble(100000, "RUB");
    
    EXPECT_CALL(*mockDelegate_, getPortfolio("acc-1"))
        .Times(2)
        .WillRepeatedly(Return(portfolio));

    cachedGateway_->getPortfolio("acc-1");
    cachedGateway_->getPortfolio("acc-1");
}

TEST_F(CachedBrokerGatewayTest, GetOrders_NeverCaches) {
    std::vector<domain::Order> orders;
    
    EXPECT_CALL(*mockDelegate_, getOrders("acc-1"))
        .Times(2)
        .WillRepeatedly(Return(orders));

    cachedGateway_->getOrders("acc-1");
    cachedGateway_->getOrders("acc-1");
}

// ============================================================================
// ТЕСТЫ: Управление кэшем
// ============================================================================

TEST_F(CachedBrokerGatewayTest, ClearQuoteCache_InvalidatesEntries) {
    auto sberQuote = createQuote("BBG004730N88", "SBER", 280.0);
    
    EXPECT_CALL(*mockDelegate_, getQuote("BBG004730N88"))
        .Times(2)
        .WillRepeatedly(Return(sberQuote));

    cachedGateway_->getQuote("BBG004730N88");
    cachedGateway_->clearQuoteCache();
    cachedGateway_->getQuote("BBG004730N88");
}

TEST_F(CachedBrokerGatewayTest, ClearInstrumentCache_InvalidatesEntries) {
    auto sber = createInstrument("BBG004730N88", "SBER", "Сбербанк");
    
    EXPECT_CALL(*mockDelegate_, getInstrumentByFigi("BBG004730N88"))
        .Times(2)
        .WillRepeatedly(Return(sber));

    cachedGateway_->getInstrumentByFigi("BBG004730N88");
    cachedGateway_->clearInstrumentCache();
    cachedGateway_->getInstrumentByFigi("BBG004730N88");
}

