/**
 * @file GetQuotesHandlerTest.cpp
 * @brief Unit-тесты для GetQuotesHandler
 *
 * GET /api/v1/quotes?figis=... — котировки по списку FIGI
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/GetQuotesHandler.hpp"
#include "ports/input/IMarketService.hpp"

#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>
#include <nlohmann/json.hpp>

using namespace trading;
using namespace trading::adapters::primary;
using ::testing::_;
using ::testing::Return;
using ::testing::Throw;

// ============================================================================
// Mock IMarketService
// ============================================================================

class MockMarketService : public ports::input::IMarketService
{
public:
    MOCK_METHOD(std::optional<domain::Quote>, getQuote, (const std::string &), (override));
    MOCK_METHOD(std::vector<domain::Quote>, getQuotes, (const std::vector<std::string> &), (override));
    MOCK_METHOD(std::vector<domain::Instrument>, getAllInstruments, (), (override));
    MOCK_METHOD(std::vector<domain::Instrument>, searchInstruments, (const std::string &), (override));
    MOCK_METHOD(std::optional<domain::Instrument>, getInstrumentByFigi, (const std::string &), (override));
};

// ============================================================================
// Test Fixture
// ============================================================================

class GetQuotesHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockService_ = std::make_shared<MockMarketService>();
        handler_ = std::make_unique<GetQuotesHandler>(mockService_);
    }

    SimpleRequest createRequest(const std::string &method,
                                const std::string &fullPath)
    {
        SimpleRequest req;
        req.setMethod(method);

        auto pos = fullPath.find('?');
        if (pos == std::string::npos)
        {
            req.setPath(fullPath);
        }
        else
        {
            req.setPath(fullPath.substr(0, pos));
            parseQueryString(req, fullPath.substr(pos + 1));
        }

        return req;
    }

    domain::Quote createQuote(const std::string &figi,
                              const std::string &ticker,
                              double lastPrice)
    {
        domain::Quote quote;
        quote.figi = figi;
        quote.ticker = ticker;
        quote.lastPrice = domain::Money::fromDouble(lastPrice, "RUB");
        quote.bidPrice = domain::Money::fromDouble(lastPrice - 0.5, "RUB");
        quote.askPrice = domain::Money::fromDouble(lastPrice + 0.5, "RUB");
        return quote;
    }

    nlohmann::json parseJson(const std::string &body)
    {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockMarketService> mockService_;
    std::unique_ptr<GetQuotesHandler> handler_;

private:
    void parseQueryString(SimpleRequest &req, const std::string &query)
    {
        std::istringstream iss(query);
        std::string pair;
        while (std::getline(iss, pair, '&'))
        {
            auto eqPos = pair.find('=');
            if (eqPos != std::string::npos)
            {
                req.setQueryParam(pair.substr(0, eqPos), pair.substr(eqPos + 1));
            }
        }
    }
};

// ============================================================================
// ТЕСТЫ: GET /api/v1/quotes
// ============================================================================

TEST_F(GetQuotesHandlerTest, SingleFigi_ReturnsQuote)
{
    std::vector<domain::Quote> quotes = {
        createQuote("BBG004730N88", "SBER", 270.0)};

    EXPECT_CALL(*mockService_, getQuotes(std::vector<std::string>{"BBG004730N88"}))
        .Times(1)
        .WillOnce(Return(quotes));

    auto req = createRequest("GET", "/api/v1/quotes?figis=BBG004730N88");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("quotes"));
    EXPECT_EQ(json["quotes"].size(), 1);
    EXPECT_EQ(json["quotes"][0]["figi"], "BBG004730N88");
    EXPECT_EQ(json["quotes"][0]["ticker"], "SBER");
}

TEST_F(GetQuotesHandlerTest, MultipleFigis_ReturnsAllQuotes)
{
    std::vector<domain::Quote> quotes = {
        createQuote("BBG004730N88", "SBER", 270.0),
        createQuote("BBG006L8G4H1", "YNDX", 3500.0)};

    EXPECT_CALL(*mockService_, getQuotes(_))
        .Times(1)
        .WillOnce(Return(quotes));

    auto req = createRequest("GET", "/api/v1/quotes?figis=BBG004730N88,BBG006L8G4H1");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["quotes"].size(), 2);
}

TEST_F(GetQuotesHandlerTest, QuoteFields_AllPresent)
{
    std::vector<domain::Quote> quotes = {
        createQuote("BBG004730N88", "SBER", 270.0)};

    EXPECT_CALL(*mockService_, getQuotes(_))
        .Times(1)
        .WillOnce(Return(quotes));

    auto req = createRequest("GET", "/api/v1/quotes?figis=BBG004730N88");
    SimpleResponse res;

    handler_->handle(req, res);

    auto json = parseJson(res.getBody());
    auto &quote = json["quotes"][0];

    EXPECT_TRUE(quote.contains("figi"));
    EXPECT_TRUE(quote.contains("ticker"));
    EXPECT_TRUE(quote.contains("last_price"));
    EXPECT_TRUE(quote.contains("bid_price"));
    EXPECT_TRUE(quote.contains("ask_price"));
    EXPECT_TRUE(quote.contains("currency"));
}

TEST_F(GetQuotesHandlerTest, MissingFigis_Returns400)
{
    auto req = createRequest("GET", "/api/v1/quotes");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["error"].get<std::string>().find("figis") != std::string::npos);
}

TEST_F(GetQuotesHandlerTest, EmptyFigis_Returns400)
{
    auto req = createRequest("GET", "/api/v1/quotes?figis=");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);
}

TEST_F(GetQuotesHandlerTest, PostMethod_Returns405)
{
    auto req = createRequest("POST", "/api/v1/quotes?figis=BBG004730N88");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(GetQuotesHandlerTest, ServiceThrows_Returns500)
{
    EXPECT_CALL(*mockService_, getQuotes(_))
        .Times(1)
        .WillOnce(Throw(std::runtime_error("Service unavailable")));

    auto req = createRequest("GET", "/api/v1/quotes?figis=BBG004730N88");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 500);
}
