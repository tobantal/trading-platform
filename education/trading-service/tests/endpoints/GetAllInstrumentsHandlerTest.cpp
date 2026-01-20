/**
 * @file GetAllInstrumentsHandlerTest.cpp
 * @brief Unit-тесты для GetAllInstrumentsHandler
 *
 * GET /api/v1/instruments — список всех инструментов
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/GetAllInstrumentsHandler.hpp"
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

class GetAllInstrumentsHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockService_ = std::make_shared<MockMarketService>();
        handler_ = std::make_unique<GetAllInstrumentsHandler>(mockService_);
    }

    SimpleRequest createRequest(const std::string &method,
                                const std::string &path)
    {
        SimpleRequest req;
        req.setMethod(method);
        req.setPath(path);
        return req;
    }

    domain::Instrument createInstrument(const std::string &figi,
                                        const std::string &ticker,
                                        const std::string &name,
                                        int lot = 10)
    {
        domain::Instrument instr;
        instr.figi = figi;
        instr.ticker = ticker;
        instr.name = name;
        instr.currency = "RUB";
        instr.lot = lot;
        return instr;
    }

    nlohmann::json parseJson(const std::string &body)
    {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockMarketService> mockService_;
    std::unique_ptr<GetAllInstrumentsHandler> handler_;
};

// ============================================================================
// ТЕСТЫ: GET /api/v1/instruments
// ============================================================================

TEST_F(GetAllInstrumentsHandlerTest, ReturnsInstrumentsArray)
{
    std::vector<domain::Instrument> instruments = {
        createInstrument("BBG004730N88", "SBER", "Сбербанк"),
        createInstrument("BBG006L8G4H1", "YNDX", "Яндекс")};

    EXPECT_CALL(*mockService_, getAllInstruments())
        .Times(1)
        .WillOnce(Return(instruments));

    auto req = createRequest("GET", "/api/v1/instruments");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("instruments"));
    EXPECT_EQ(json["instruments"].size(), 2);
}

TEST_F(GetAllInstrumentsHandlerTest, InstrumentFields_AllPresent)
{
    std::vector<domain::Instrument> instruments = {
        createInstrument("BBG004730N88", "SBER", "Сбербанк", 10)};

    EXPECT_CALL(*mockService_, getAllInstruments())
        .Times(1)
        .WillOnce(Return(instruments));

    auto req = createRequest("GET", "/api/v1/instruments");
    SimpleResponse res;

    handler_->handle(req, res);

    auto json = parseJson(res.getBody());
    auto &instr = json["instruments"][0];

    EXPECT_EQ(instr["figi"], "BBG004730N88");
    EXPECT_EQ(instr["ticker"], "SBER");
    EXPECT_EQ(instr["name"], "Сбербанк");
    EXPECT_EQ(instr["currency"], "RUB");
    EXPECT_EQ(instr["lot"], 10);
}

TEST_F(GetAllInstrumentsHandlerTest, EmptyList_ReturnsEmptyArray)
{
    EXPECT_CALL(*mockService_, getAllInstruments())
        .Times(1)
        .WillOnce(Return(std::vector<domain::Instrument>{}));

    auto req = createRequest("GET", "/api/v1/instruments");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["instruments"].is_array());
    EXPECT_EQ(json["instruments"].size(), 0);
}

TEST_F(GetAllInstrumentsHandlerTest, PostMethod_Returns405)
{
    auto req = createRequest("POST", "/api/v1/instruments");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(GetAllInstrumentsHandlerTest, DeleteMethod_Returns405)
{
    auto req = createRequest("DELETE", "/api/v1/instruments");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(GetAllInstrumentsHandlerTest, ServiceThrows_Returns500)
{
    EXPECT_CALL(*mockService_, getAllInstruments())
        .Times(1)
        .WillOnce(Throw(std::runtime_error("Database error")));

    auto req = createRequest("GET", "/api/v1/instruments");
    SimpleResponse res;

    handler_->handle(req, res);

    EXPECT_EQ(res.getStatus(), 500);
}
