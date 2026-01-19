/**
 * @file GetPortfolioHandlerTest.cpp
 * @brief Unit-тесты для GetPortfolioHandler
 * 
 * GET /api/v1/portfolio?account_id=xxx — полный портфель
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/GetPortfolioHandler.hpp"
#include "ports/output/IBrokerGateway.hpp"

#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>
#include <nlohmann/json.hpp>

using namespace broker;
using namespace broker::adapters::primary;
using ::testing::Return;
using ::testing::Throw;
using ::testing::_;

// ============================================================================
// Mock IBrokerGateway
// ============================================================================

class MockBrokerGateway : public ports::output::IBrokerGateway {
public:
    MOCK_METHOD(void, registerAccount, (const std::string&, const std::string&), (override));
    MOCK_METHOD(void, unregisterAccount, (const std::string&), (override));
    MOCK_METHOD(std::optional<domain::Quote>, getQuote, (const std::string&), (override));
    MOCK_METHOD(std::vector<domain::Quote>, getQuotes, (const std::vector<std::string>&), (override));
    MOCK_METHOD(std::optional<domain::Instrument>, getInstrumentByFigi, (const std::string&), (override));
    MOCK_METHOD(std::vector<domain::Instrument>, getAllInstruments, (), (override));
    MOCK_METHOD(std::vector<domain::Instrument>, searchInstruments, (const std::string&), (override));
    MOCK_METHOD(domain::Portfolio, getPortfolio, (const std::string&), (override));
    MOCK_METHOD(domain::OrderResult, placeOrder, (const std::string&, const domain::OrderRequest&), (override));
    MOCK_METHOD(bool, cancelOrder, (const std::string&, const std::string&), (override));
    MOCK_METHOD(std::vector<domain::Order>, getOrders, (const std::string&), (override));
    MOCK_METHOD(std::optional<domain::Order>, getOrderStatus, (const std::string&, const std::string&), (override));
    MOCK_METHOD(std::vector<domain::Order>, getOrderHistory,
                (const std::string&,
                 const std::optional<std::chrono::system_clock::time_point>&,
                 const std::optional<std::chrono::system_clock::time_point>&), (override));
};

// ============================================================================
// Test Fixture
// ============================================================================

class GetPortfolioHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockBroker_ = std::make_shared<MockBrokerGateway>();
        handler_ = std::make_unique<GetPortfolioHandler>(mockBroker_);
    }

    SimpleRequest createRequest(const std::string& method,
                                const std::string& fullPath,
                                const std::string& body = "") {
        SimpleRequest req;
        req.setMethod(method);
        req.setBody(body);
        
        auto pos = fullPath.find('?');
        if (pos == std::string::npos) {
            req.setPath(fullPath);
        } else {
            req.setPath(fullPath.substr(0, pos));
            parseQueryString(req, fullPath.substr(pos + 1));
        }
        
        return req;
    }

    domain::Portfolio createTestPortfolio(const std::string& accountId, double cash) {
        domain::Portfolio p;
        p.accountId = accountId;
        p.cash = domain::Money::fromDouble(cash, "RUB");
        p.totalValue = domain::Money::fromDouble(cash, "RUB");
        return p;
    }

    domain::Position createTestPosition(const std::string& figi, 
                                        const std::string& ticker,
                                        int quantity) {
        domain::Position pos;
        pos.figi = figi;
        pos.ticker = ticker;
        pos.quantity = quantity;
        pos.averagePrice = domain::Money::fromDouble(265.0, "RUB");
        pos.currentPrice = domain::Money::fromDouble(270.0, "RUB");
        pos.pnl = domain::Money::fromDouble(500.0, "RUB");
        pos.pnlPercent = 1.89;
        return pos;
    }

    nlohmann::json parseJson(const std::string& body) {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockBrokerGateway> mockBroker_;
    std::unique_ptr<GetPortfolioHandler> handler_;

private:
    void parseQueryString(SimpleRequest& req, const std::string& query) {
        std::istringstream iss(query);
        std::string pair;
        while (std::getline(iss, pair, '&')) {
            auto eqPos = pair.find('=');
            if (eqPos != std::string::npos) {
                req.setQueryParam(pair.substr(0, eqPos), pair.substr(eqPos + 1));
            }
        }
    }
};

// ============================================================================
// ТЕСТЫ: GET /api/v1/portfolio
// ============================================================================

TEST_F(GetPortfolioHandlerTest, ExistingAccount_Returns200) {
    auto portfolio = createTestPortfolio("acc-001-sandbox", 100000.0);
    
    EXPECT_CALL(*mockBroker_, getPortfolio("acc-001-sandbox"))
        .Times(1)
        .WillOnce(Return(portfolio));

    auto req = createRequest("GET", "/api/v1/portfolio?account_id=acc-001-sandbox");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["account_id"], "acc-001-sandbox");
    EXPECT_TRUE(json.contains("cash"));
    EXPECT_TRUE(json.contains("total_value"));
    EXPECT_TRUE(json.contains("positions"));
}

TEST_F(GetPortfolioHandlerTest, WithPositions_ReturnsPositionsArray) {
    auto portfolio = createTestPortfolio("acc-001-sandbox", 50000.0);
    portfolio.positions.push_back(createTestPosition("BBG004730N88", "SBER", 100));
    portfolio.positions.push_back(createTestPosition("BBG006L8G4H1", "YNDX", 50));
    
    EXPECT_CALL(*mockBroker_, getPortfolio("acc-001-sandbox"))
        .Times(1)
        .WillOnce(Return(portfolio));

    auto req = createRequest("GET", "/api/v1/portfolio?account_id=acc-001-sandbox");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["positions"].size(), 2);
    EXPECT_EQ(json["positions"][0]["ticker"], "SBER");
    EXPECT_EQ(json["positions"][1]["ticker"], "YNDX");
}

TEST_F(GetPortfolioHandlerTest, CashFormat_ReturnsUnitsNanoCurrency) {
    auto portfolio = createTestPortfolio("acc-001-sandbox", 100000.0);
    
    EXPECT_CALL(*mockBroker_, getPortfolio("acc-001-sandbox"))
        .Times(1)
        .WillOnce(Return(portfolio));

    auto req = createRequest("GET", "/api/v1/portfolio?account_id=acc-001-sandbox");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["cash"]["units"], 100000);
    EXPECT_EQ(json["cash"]["nano"], 0);
    EXPECT_EQ(json["cash"]["currency"], "RUB");
}

TEST_F(GetPortfolioHandlerTest, AccountNotFound_Returns404) {
    EXPECT_CALL(*mockBroker_, getPortfolio("unknown-account"))
        .Times(1)
        .WillOnce(Throw(std::runtime_error("Account not found")));

    auto req = createRequest("GET", "/api/v1/portfolio?account_id=unknown-account");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["error"], "Account not found");
}

TEST_F(GetPortfolioHandlerTest, MissingAccountId_Returns400) {
    auto req = createRequest("GET", "/api/v1/portfolio");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["error"].get<std::string>().find("account_id") != std::string::npos);
}

TEST_F(GetPortfolioHandlerTest, PostMethod_Returns405) {
    auto req = createRequest("POST", "/api/v1/portfolio?account_id=acc-001-sandbox");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(GetPortfolioHandlerTest, DeleteMethod_Returns405) {
    auto req = createRequest("DELETE", "/api/v1/portfolio?account_id=acc-001-sandbox");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 405);
}
