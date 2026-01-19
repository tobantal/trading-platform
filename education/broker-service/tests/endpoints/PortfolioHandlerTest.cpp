/**
 * @file PortfolioHandlerTest.cpp
 * @brief Unit-тесты для PortfolioHandler
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/PortfolioHandler.hpp"
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
// Mock для IBrokerGateway
// ============================================================================

class MockBrokerGateway : public ports::output::IBrokerGateway {
public:
    MOCK_METHOD(void, registerAccount, (const std::string&, const std::string&), (override));
    MOCK_METHOD(void, unregisterAccount, (const std::string&), (override));
    MOCK_METHOD(std::optional<domain::Quote>, getQuote, (const std::string&), (override));
    MOCK_METHOD(std::vector<domain::Quote>, getQuotes, (const std::vector<std::string>&), (override));
    MOCK_METHOD(std::vector<domain::Instrument>, searchInstruments, (const std::string&), (override));
    MOCK_METHOD(std::optional<domain::Instrument>, getInstrumentByFigi, (const std::string&), (override));
    MOCK_METHOD(std::vector<domain::Instrument>, getAllInstruments, (), (override));
    MOCK_METHOD(domain::Portfolio, getPortfolio, (const std::string&), (override));
    MOCK_METHOD(domain::OrderResult, placeOrder, (const std::string&, const domain::OrderRequest&), (override));
    MOCK_METHOD(bool, cancelOrder, (const std::string&, const std::string&), (override));
    MOCK_METHOD(std::optional<domain::Order>, getOrderStatus, (const std::string&, const std::string&), (override));
    MOCK_METHOD(std::vector<domain::Order>, getOrders, (const std::string&), (override));
    MOCK_METHOD((std::vector<domain::Order>), getOrderHistory,
                (const std::string&, const std::optional<std::chrono::system_clock::time_point>&,
                 const std::optional<std::chrono::system_clock::time_point>&), (override));
};

// ============================================================================
// Тестовый класс
// ============================================================================

class PortfolioHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockBroker_ = std::make_shared<MockBrokerGateway>();
        handler_ = std::make_unique<PortfolioHandler>(mockBroker_);
    }

    /**
     * @brief Хелпер для создания запроса с парсингом query string
     */
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
        p.totalValue = p.cash;
        return p;
    }

    nlohmann::json parseJson(const std::string& body) {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockBrokerGateway> mockBroker_;
    std::unique_ptr<PortfolioHandler> handler_;

private:
    void parseQueryString(SimpleRequest& req, const std::string& query) {
        size_t start = 0;
        while (start < query.size()) {
            auto eq = query.find('=', start);
            auto amp = query.find('&', start);
            if (eq == std::string::npos) break;

            std::string key = query.substr(start, eq - start);
            std::string value = (amp == std::string::npos)
                ? query.substr(eq + 1)
                : query.substr(eq + 1, amp - eq - 1);

            if (!key.empty()) {
                req.setQueryParam(key, value);
            }

            if (amp == std::string::npos) break;
            start = amp + 1;
        }
    }
};

// ============================================================================
// ТЕСТЫ: GET /api/v1/portfolio
// ============================================================================

TEST_F(PortfolioHandlerTest, GetPortfolio_ExistingAccount_Returns200) {
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
    EXPECT_TRUE(json.contains("positions"));
}

TEST_F(PortfolioHandlerTest, GetPortfolio_NonSandboxAccount_Returns404) {
    EXPECT_CALL(*mockBroker_, getPortfolio("unknown-real-account"))
        .Times(1)
        .WillOnce(Throw(std::runtime_error("Account not found")));

    auto req = createRequest("GET", "/api/v1/portfolio?account_id=unknown-real-account");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
}

TEST_F(PortfolioHandlerTest, GetPortfolio_MissingAccountId_Returns400) {
    auto req = createRequest("GET", "/api/v1/portfolio");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("error"));
}

// ============================================================================
// ТЕСТЫ: GET /api/v1/portfolio/positions
// ============================================================================

TEST_F(PortfolioHandlerTest, GetPositions_ReturnsArray) {
    domain::Portfolio portfolio;
    portfolio.accountId = "acc-001-sandbox";
    portfolio.cash = domain::Money::fromDouble(50000.0, "RUB");
    
    domain::Position pos;
    pos.figi = "BBG004730N88";
    pos.ticker = "SBER";
    pos.quantity = 100;
    pos.averagePrice = domain::Money::fromDouble(265.0, "RUB");
    pos.currentPrice = domain::Money::fromDouble(270.0, "RUB");
    pos.pnl = domain::Money::fromDouble(500.0, "RUB");
    pos.pnlPercent = 1.89;
    portfolio.positions.push_back(pos);
    
    EXPECT_CALL(*mockBroker_, getPortfolio("acc-001-sandbox"))
        .Times(1)
        .WillOnce(Return(portfolio));

    auto req = createRequest("GET", "/api/v1/portfolio/positions?account_id=acc-001-sandbox");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 1);
    EXPECT_EQ(json[0]["figi"], "BBG004730N88");
    EXPECT_EQ(json[0]["ticker"], "SBER");
}

// ============================================================================
// ТЕСТЫ: GET /api/v1/portfolio/cash
// ============================================================================

TEST_F(PortfolioHandlerTest, GetCash_ReturnsUnitsFormat) {
    auto portfolio = createTestPortfolio("acc-001-sandbox", 100000.0);
    
    EXPECT_CALL(*mockBroker_, getPortfolio("acc-001-sandbox"))
        .Times(1)
        .WillOnce(Return(portfolio));

    auto req = createRequest("GET", "/api/v1/portfolio/cash?account_id=acc-001-sandbox");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("units"));
    EXPECT_TRUE(json.contains("currency"));
    EXPECT_EQ(json["units"], 100000);
    EXPECT_EQ(json["currency"], "RUB");
}

// ============================================================================
// ТЕСТЫ: Неизвестные пути
// ============================================================================

TEST_F(PortfolioHandlerTest, UnknownPath_Returns404) {
    auto req = createRequest("GET", "/api/v1/portfolio/unknown?account_id=acc-001-sandbox");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
}