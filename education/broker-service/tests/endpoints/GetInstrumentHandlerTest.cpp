/**
 * @file GetInstrumentHandlerTest.cpp
 * @brief Unit-тесты для GetInstrumentHandler
 * 
 * GET /api/v1/instruments/{figi} — инструмент по FIGI
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/GetInstrumentHandler.hpp"
#include "ports/input/IQuoteService.hpp"

#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>
#include <nlohmann/json.hpp>

using namespace broker;
using namespace broker::adapters::primary;
using ::testing::Return;
using ::testing::Throw;
using ::testing::_;

// ============================================================================
// Mock IQuoteService
// ============================================================================

class MockQuoteService : public ports::input::IQuoteService {
public:
    MOCK_METHOD(std::optional<domain::Quote>, getQuote, (const std::string&), (override));
    MOCK_METHOD(std::optional<domain::Instrument>, getInstrument, (const std::string&), (override));
    MOCK_METHOD(std::vector<domain::Instrument>, getInstruments, (), (override));
};

// ============================================================================
// Test Fixture
// ============================================================================

class GetInstrumentHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockService_ = std::make_shared<MockQuoteService>();
        handler_ = std::make_unique<GetInstrumentHandler>(mockService_);
    }

    SimpleRequest createRequest(const std::string& method,
                                const std::string& path,
                                const std::string& pathPattern = "") {
        SimpleRequest req;
        req.setMethod(method);
        req.setPath(path);
        
        if (!pathPattern.empty()) {
            req.setPathPattern(pathPattern);
        }
        
        return req;
    }

    domain::Instrument createInstrument(const std::string& figi,
                                        const std::string& ticker,
                                        const std::string& name,
                                        int lot = 10) {
        domain::Instrument instr;
        instr.figi = figi;
        instr.ticker = ticker;
        instr.name = name;
        instr.currency = "RUB";
        instr.lot = lot;
        return instr;
    }

    nlohmann::json parseJson(const std::string& body) {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockQuoteService> mockService_;
    std::unique_ptr<GetInstrumentHandler> handler_;
};

// ============================================================================
// ТЕСТЫ: GET /api/v1/instruments/{figi}
// ============================================================================

TEST_F(GetInstrumentHandlerTest, Found_Returns200) {
    auto instrument = createInstrument("BBG004730N88", "SBER", "Сбербанк", 10);
    
    EXPECT_CALL(*mockService_, getInstrument("BBG004730N88"))
        .Times(1)
        .WillOnce(Return(instrument));

    auto req = createRequest("GET", 
                             "/api/v1/instruments/BBG004730N88",
                             "/api/v1/instruments/*");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["figi"], "BBG004730N88");
    EXPECT_EQ(json["ticker"], "SBER");
    EXPECT_EQ(json["name"], "Сбербанк");
    EXPECT_EQ(json["currency"], "RUB");
    EXPECT_EQ(json["lot"], 10);
}

TEST_F(GetInstrumentHandlerTest, NotFound_Returns404) {
    EXPECT_CALL(*mockService_, getInstrument("UNKNOWN_FIGI"))
        .Times(1)
        .WillOnce(Return(std::nullopt));

    auto req = createRequest("GET", 
                             "/api/v1/instruments/UNKNOWN_FIGI",
                             "/api/v1/instruments/*");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["error"], "Instrument not found");
}

TEST_F(GetInstrumentHandlerTest, MissingFigi_Returns400) {
    // pathPattern не установлен — getPathParam вернёт nullopt
    auto req = createRequest("GET", "/api/v1/instruments/");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["error"].get<std::string>().find("FIGI") != std::string::npos);
}

TEST_F(GetInstrumentHandlerTest, DifferentFigi_CallsServiceCorrectly) {
    auto instrument = createInstrument("BBG006L8G4H1", "YNDX", "Яндекс", 1);
    
    EXPECT_CALL(*mockService_, getInstrument("BBG006L8G4H1"))
        .Times(1)
        .WillOnce(Return(instrument));

    auto req = createRequest("GET", 
                             "/api/v1/instruments/BBG006L8G4H1",
                             "/api/v1/instruments/*");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["ticker"], "YNDX");
}

TEST_F(GetInstrumentHandlerTest, PostMethod_Returns405) {
    auto req = createRequest("POST", "/api/v1/instruments/BBG004730N88");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(GetInstrumentHandlerTest, DeleteMethod_Returns405) {
    auto req = createRequest("DELETE", "/api/v1/instruments/BBG004730N88");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(GetInstrumentHandlerTest, PutMethod_Returns405) {
    auto req = createRequest("PUT", "/api/v1/instruments/BBG004730N88");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(GetInstrumentHandlerTest, ServiceThrows_Returns500) {
    EXPECT_CALL(*mockService_, getInstrument("BBG004730N88"))
        .Times(1)
        .WillOnce(Throw(std::runtime_error("Service unavailable")));

    auto req = createRequest("GET", 
                             "/api/v1/instruments/BBG004730N88",
                             "/api/v1/instruments/*");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 500);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("error"));
}
