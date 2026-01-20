/**
 * @file SearchInstrumentsHandlerTest.cpp
 * @brief Unit-тесты для SearchInstrumentsHandler
 *
 * GET /api/v1/instruments/search?query=... — поиск инструментов
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/SearchInstrumentsHandler.hpp"
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
    MOCK_METHOD(std::optional<domain::Quote>, getQuote, (const std::string&), (override));
    MOCK_METHOD(std::vector<domain::Quote>, getQuotes, (const std::vector<std::string> &), (override));
    MOCK_METHOD(std::vector<domain::Instrument>, getAllInstruments, (), (override));
    MOCK_METHOD(std::vector<domain::Instrument>, searchInstruments, (const std::string &), (override));
    MOCK_METHOD(std::optional<domain::Instrument>, getInstrumentByFigi, (const std::string &), (override));
};

// ============================================================================
// Test Fixture
// ============================================================================

class SearchInstrumentsHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockService_ = std::make_shared<MockMarketService>();
        handler_ = std::make_unique<SearchInstrumentsHandler>(mockService_);
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

    domain::Instrument createInstrument(const std::string &figi,
                                        const std::string &ticker,
                                        const std::string &name)
    {
        domain::Instrument instr;
        instr.figi = figi;
        instr.ticker = ticker;
        instr.name = name;
        instr.currency = "RUB";
        instr.lot = 10;
        return instr;
    }

    nlohmann::json parseJson(const std::string &body)
    {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockMarketService> mockService_;
    std::unique_ptr<SearchInstrumentsHandler> handler_;

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
// ТЕСТЫ: GET /api/v1/instruments/search
// ============================================================================

TEST_F(SearchInstrumentsHandlerTest, SearchByTicker_ReturnsMatches)
{
    std::vector<domain::Instrument> instruments = {
        createInstrument("BBG004730N88", "SBER", "Сбербанк")};

    EXPECT_CALL(*mockService_, searchInstruments("SBER"))
        .Times(1)
        .WillOnce(Return(instruments));

    auto req = createRequest("GET", "/api/v1/instruments/search?query=SBER");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("instruments"));
    EXPECT_EQ(json["instruments"].size(), 1);
    EXPECT_EQ(json["instruments"][0]["ticker"], "SBER");
}

TEST_F(SearchInstrumentsHandlerTest, SearchByName_ReturnsMatches)
{
    std::vector<domain::Instrument> instruments = {
        createInstrument("BBG004730N88", "SBER", "Сбербанк"),
        createInstrument("BBG000000001", "SBERP", "Сбербанк Привилегированные")};

    EXPECT_CALL(*mockService_, searchInstruments("Сбер"))
        .Times(1)
        .WillOnce(Return(instruments));

    auto req = createRequest("GET", "/api/v1/instruments/search?query=Сбер");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["instruments"].size(), 2);
}

TEST_F(SearchInstrumentsHandlerTest, NoMatches_ReturnsEmptyArray)
{
    EXPECT_CALL(*mockService_, searchInstruments("UNKNOWN"))
        .Times(1)
        .WillOnce(Return(std::vector<domain::Instrument>{}));

    auto req = createRequest("GET", "/api/v1/instruments/search?query=UNKNOWN");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["instruments"].is_array());
    EXPECT_EQ(json["instruments"].size(), 0);
}

TEST_F(SearchInstrumentsHandlerTest, MissingQuery_Returns400)
{
    auto req = createRequest("GET", "/api/v1/instruments/search");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["error"].get<std::string>().find("query") != std::string::npos);
}

TEST_F(SearchInstrumentsHandlerTest, EmptyQuery_Returns400)
{
    auto req = createRequest("GET", "/api/v1/instruments/search?query=");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 400);
}

TEST_F(SearchInstrumentsHandlerTest, PostMethod_Returns405)
{
    auto req = createRequest("POST", "/api/v1/instruments/search?query=SBER");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(SearchInstrumentsHandlerTest, ServiceThrows_Returns500)
{
    EXPECT_CALL(*mockService_, searchInstruments(_))
        .Times(1)
        .WillOnce(Throw(std::runtime_error("Search failed")));

    auto req = createRequest("GET", "/api/v1/instruments/search?query=SBER");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 500);
}
