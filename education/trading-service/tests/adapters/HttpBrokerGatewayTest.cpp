#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/secondary/HttpBrokerGateway.hpp"
#include "settings/IBrokerClientSettings.hpp"
#include <IHttpClient.hpp>
#include <SimpleResponse.hpp>

using namespace trading;
using namespace trading::adapters::secondary;
using ::testing::_;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::SetArgReferee;

// ============================================================================
// Mocks
// ============================================================================

class MockHttpClient : public IHttpClient {
public:
    MOCK_METHOD(bool, send, (const IRequest& req, IResponse& res), (override));
};

class MockBrokerClientSettings : public settings::IBrokerClientSettings {
public:
    std::string getHost() const override { return "broker-service"; }
    int getPort() const override { return 8083; }
};

// ============================================================================
// Test Fixture
// ============================================================================

class HttpBrokerGatewayTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockHttpClient_ = std::make_shared<MockHttpClient>();
        mockSettings_ = std::make_shared<MockBrokerClientSettings>();
        gateway_ = std::make_shared<HttpBrokerGateway>(mockHttpClient_, mockSettings_);
    }

    // Хелпер для настройки mock ответа
    void expectGetRequest(const std::string& expectedPath, int status, const std::string& body) {
        EXPECT_CALL(*mockHttpClient_, send(_, _))
            .WillOnce([expectedPath, status, body](const IRequest& req, IResponse& res) {
                EXPECT_EQ(req.getPath(), expectedPath);
                EXPECT_EQ(req.getMethod(), "GET");
                
                // SimpleResponse нужно кастить для setStatus/setBody
                auto& simpleRes = dynamic_cast<SimpleResponse&>(res);
                simpleRes.setStatus(status);
                simpleRes.setBody(body);
                return true;
            });
    }

    std::shared_ptr<MockHttpClient> mockHttpClient_;
    std::shared_ptr<MockBrokerClientSettings> mockSettings_;
    std::shared_ptr<HttpBrokerGateway> gateway_;
};

// ============================================================================
// ТЕСТЫ: getAllInstruments
// ============================================================================

TEST_F(HttpBrokerGatewayTest, GetAllInstruments_ArrayResponse_ParsesCorrectly) {
    // Broker возвращает массив напрямую (реальное поведение)
    std::string response = R"([
        {"figi":"BBG004730N88","ticker":"SBER","name":"Сбербанк","currency":"RUB","lot":10},
        {"figi":"BBG004730RP0","ticker":"GAZP","name":"Газпром","currency":"RUB","lot":10}
    ])";
    
    expectGetRequest("/api/v1/instruments", 200, response);
    
    auto instruments = gateway_->getAllInstruments();
    
    ASSERT_EQ(instruments.size(), 2);
    EXPECT_EQ(instruments[0].figi, "BBG004730N88");
    EXPECT_EQ(instruments[0].ticker, "SBER");
    EXPECT_EQ(instruments[0].name, "Сбербанк");
    EXPECT_EQ(instruments[1].figi, "BBG004730RP0");
    EXPECT_EQ(instruments[1].ticker, "GAZP");
}

TEST_F(HttpBrokerGatewayTest, GetAllInstruments_ObjectResponse_ParsesCorrectly) {
    // Альтернативный формат с полем "instruments"
    std::string response = R"({
        "instruments": [
            {"figi":"BBG004730N88","ticker":"SBER","name":"Сбербанк","currency":"RUB","lot":10}
        ]
    })";
    
    expectGetRequest("/api/v1/instruments", 200, response);
    
    auto instruments = gateway_->getAllInstruments();
    
    ASSERT_EQ(instruments.size(), 1);
    EXPECT_EQ(instruments[0].figi, "BBG004730N88");
}

TEST_F(HttpBrokerGatewayTest, GetAllInstruments_EmptyArray_ReturnsEmpty) {
    expectGetRequest("/api/v1/instruments", 200, "[]");
    
    auto instruments = gateway_->getAllInstruments();
    
    EXPECT_TRUE(instruments.empty());
}

TEST_F(HttpBrokerGatewayTest, GetAllInstruments_Error500_ReturnsEmpty) {
    expectGetRequest("/api/v1/instruments", 500, "");
    
    auto instruments = gateway_->getAllInstruments();
    
    EXPECT_TRUE(instruments.empty());
}

// ============================================================================
// ТЕСТЫ: searchInstruments
// NOTE: searchInstruments вызывает getAllInstruments и фильтрует локально,
//       т.к. broker-service не поддерживает search endpoint.
// ============================================================================

TEST_F(HttpBrokerGatewayTest, SearchInstruments_FiltersByTicker) {
    // Возвращаем все инструменты, searchInstruments фильтрует по ticker
    std::string response = R"([
        {"figi":"BBG004730N88","ticker":"SBER","name":"Сбербанк","currency":"RUB","lot":10},
        {"figi":"BBG004730RP0","ticker":"GAZP","name":"Газпром","currency":"RUB","lot":10}
    ])";
    
    expectGetRequest("/api/v1/instruments", 200, response);
    
    auto instruments = gateway_->searchInstruments("SBER");
    
    ASSERT_EQ(instruments.size(), 1);
    EXPECT_EQ(instruments[0].ticker, "SBER");
}

TEST_F(HttpBrokerGatewayTest, SearchInstruments_FiltersByName) {
    std::string response = R"([
        {"figi":"BBG004730N88","ticker":"SBER","name":"Сбербанк","currency":"RUB","lot":10},
        {"figi":"BBG004730RP0","ticker":"GAZP","name":"Газпром","currency":"RUB","lot":10}
    ])";
    
    expectGetRequest("/api/v1/instruments", 200, response);
    
    auto instruments = gateway_->searchInstruments("Газпром");
    
    ASSERT_EQ(instruments.size(), 1);
    EXPECT_EQ(instruments[0].ticker, "GAZP");
}

TEST_F(HttpBrokerGatewayTest, SearchInstruments_CaseInsensitive) {
    std::string response = R"([
        {"figi":"BBG004730N88","ticker":"SBER","name":"Сбербанк","currency":"RUB","lot":10}
    ])";
    
    expectGetRequest("/api/v1/instruments", 200, response);
    
    auto instruments = gateway_->searchInstruments("sber");
    
    ASSERT_EQ(instruments.size(), 1);
    EXPECT_EQ(instruments[0].ticker, "SBER");
}

TEST_F(HttpBrokerGatewayTest, SearchInstruments_NoResults_ReturnsEmpty) {
    std::string response = R"([
        {"figi":"BBG004730N88","ticker":"SBER","name":"Сбербанк","currency":"RUB","lot":10}
    ])";
    
    expectGetRequest("/api/v1/instruments", 200, response);
    
    auto instruments = gateway_->searchInstruments("UNKNOWN");
    
    EXPECT_TRUE(instruments.empty());
}

TEST_F(HttpBrokerGatewayTest, SearchInstruments_EmptyQuery_ReturnsAll) {
    std::string response = R"([
        {"figi":"BBG004730N88","ticker":"SBER","name":"Сбербанк","currency":"RUB","lot":10},
        {"figi":"BBG004730RP0","ticker":"GAZP","name":"Газпром","currency":"RUB","lot":10}
    ])";
    
    expectGetRequest("/api/v1/instruments", 200, response);
    
    auto instruments = gateway_->searchInstruments("");
    
    ASSERT_EQ(instruments.size(), 2);
}

// ============================================================================
// ТЕСТЫ: getInstrumentByFigi
// ============================================================================

TEST_F(HttpBrokerGatewayTest, GetInstrumentByFigi_Found_ReturnsInstrument) {
    std::string response = R"({"figi":"BBG004730N88","ticker":"SBER","name":"Сбербанк","currency":"RUB","lot":10})";
    
    expectGetRequest("/api/v1/instruments/BBG004730N88", 200, response);
    
    auto instrument = gateway_->getInstrumentByFigi("BBG004730N88");
    
    ASSERT_TRUE(instrument.has_value());
    EXPECT_EQ(instrument->figi, "BBG004730N88");
    EXPECT_EQ(instrument->ticker, "SBER");
}

TEST_F(HttpBrokerGatewayTest, GetInstrumentByFigi_NotFound_ReturnsNullopt) {
    expectGetRequest("/api/v1/instruments/UNKNOWN", 404, "");
    
    auto instrument = gateway_->getInstrumentByFigi("UNKNOWN");
    
    EXPECT_FALSE(instrument.has_value());
}

// ============================================================================
// ТЕСТЫ: getQuotes
// ============================================================================

TEST_F(HttpBrokerGatewayTest, GetQuotes_ArrayResponse_ParsesCorrectly) {
    std::string response = R"([
        {"figi":"BBG004730N88","ticker":"SBER","last_price":280.5,"bid_price":280.0,"ask_price":281.0,"currency":"RUB"}
    ])";
    
    expectGetRequest("/api/v1/quotes?figis=BBG004730N88", 200, response);
    
    auto quotes = gateway_->getQuotes({"BBG004730N88"});
    
    ASSERT_EQ(quotes.size(), 1);
    EXPECT_EQ(quotes[0].figi, "BBG004730N88");
    EXPECT_EQ(quotes[0].ticker, "SBER");
}

TEST_F(HttpBrokerGatewayTest, GetQuotes_ObjectResponse_ParsesCorrectly) {
    std::string response = R"({
        "quotes": [
            {"figi":"BBG004730N88","ticker":"SBER","last_price":280.5,"bid_price":280.0,"ask_price":281.0,"currency":"RUB"}
        ]
    })";
    
    expectGetRequest("/api/v1/quotes?figis=BBG004730N88", 200, response);
    
    auto quotes = gateway_->getQuotes({"BBG004730N88"});
    
    ASSERT_EQ(quotes.size(), 1);
    EXPECT_EQ(quotes[0].figi, "BBG004730N88");
}

TEST_F(HttpBrokerGatewayTest, GetQuotes_MultipleFigis_CorrectPath) {
    std::string response = R"([])";
    
    expectGetRequest("/api/v1/quotes?figis=BBG004730N88,BBG004730RP0", 200, response);
    
    auto quotes = gateway_->getQuotes({"BBG004730N88", "BBG004730RP0"});
    
    EXPECT_TRUE(quotes.empty());
}

// ============================================================================
// ТЕСТЫ: getQuote (single)
// ============================================================================

TEST_F(HttpBrokerGatewayTest, GetQuote_Found_ReturnsQuote) {
    std::string response = R"([
        {"figi":"BBG004730N88","ticker":"SBER","last_price":280.5,"bid_price":280.0,"ask_price":281.0,"currency":"RUB"}
    ])";
    
    expectGetRequest("/api/v1/quotes?figis=BBG004730N88", 200, response);
    
    auto quote = gateway_->getQuote("BBG004730N88");
    
    ASSERT_TRUE(quote.has_value());
    EXPECT_EQ(quote->figi, "BBG004730N88");
}

TEST_F(HttpBrokerGatewayTest, GetQuote_NotFound_ReturnsNullopt) {
    expectGetRequest("/api/v1/quotes?figis=UNKNOWN", 200, "[]");
    
    auto quote = gateway_->getQuote("UNKNOWN");
    
    EXPECT_FALSE(quote.has_value());
}

// ============================================================================
// ТЕСТЫ: getOrders
// ============================================================================

TEST_F(HttpBrokerGatewayTest, GetOrders_ArrayResponse_ParsesCorrectly) {
    std::string response = R"([
        {"id":"order-1","account_id":"acc-1","figi":"BBG004730N88","direction":"BUY","type":"MARKET","quantity":10,"price":280.0,"status":"PENDING","currency":"RUB"}
    ])";
    
    expectGetRequest("/api/v1/orders?account_id=acc-1", 200, response);
    
    auto orders = gateway_->getOrders("acc-1");
    
    ASSERT_EQ(orders.size(), 1);
    EXPECT_EQ(orders[0].id, "order-1");
}

TEST_F(HttpBrokerGatewayTest, GetOrders_ObjectResponse_ParsesCorrectly) {
    std::string response = R"({
        "orders": [
            {"id":"order-1","account_id":"acc-1","figi":"BBG004730N88","direction":"BUY","type":"MARKET","quantity":10,"price":280.0,"status":"PENDING","currency":"RUB"}
        ]
    })";
    
    expectGetRequest("/api/v1/orders?account_id=acc-1", 200, response);
    
    auto orders = gateway_->getOrders("acc-1");
    
    ASSERT_EQ(orders.size(), 1);
    EXPECT_EQ(orders[0].id, "order-1");
}
