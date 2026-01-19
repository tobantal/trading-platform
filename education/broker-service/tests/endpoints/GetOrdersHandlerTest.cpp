/**
 * @file GetOrdersHandlerTest.cpp
 * @brief Unit-тесты для GetOrdersHandler
 * 
 * GET /api/v1/orders?account_id=xxx — список ордеров
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/GetOrdersHandler.hpp"
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

class GetOrdersHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockBroker_ = std::make_shared<MockBrokerGateway>();
        handler_ = std::make_unique<GetOrdersHandler>(mockBroker_);
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

    nlohmann::json parseJson(const std::string& body) {
        return nlohmann::json::parse(body);
    }

    domain::Order createOrder(const std::string& id, 
                              const std::string& accountId,
                              domain::OrderStatus status = domain::OrderStatus::FILLED) {
        domain::Order order;
        order.id = id;
        order.accountId = accountId;
        order.figi = "BBG004730N88";
        order.direction = domain::OrderDirection::BUY;
        order.type = domain::OrderType::MARKET;
        order.quantity = 10;
        order.status = status;
        return order;
    }

    std::shared_ptr<MockBrokerGateway> mockBroker_;
    std::unique_ptr<GetOrdersHandler> handler_;

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
// ТЕСТЫ: GET /api/v1/orders
// ============================================================================

TEST_F(GetOrdersHandlerTest, ReturnsOrdersArray) {
    std::vector<domain::Order> orders = {
        createOrder("ord-111", "acc-001-sandbox"),
        createOrder("ord-222", "acc-001-sandbox", domain::OrderStatus::PENDING)
    };
    
    EXPECT_CALL(*mockBroker_, getOrders("acc-001-sandbox"))
        .Times(1)
        .WillOnce(Return(orders));

    auto req = createRequest("GET", "/api/v1/orders?account_id=acc-001-sandbox");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 2);
    EXPECT_EQ(json[0]["order_id"], "ord-111");
    EXPECT_EQ(json[1]["order_id"], "ord-222");
}

TEST_F(GetOrdersHandlerTest, EmptyList_Returns200) {
    EXPECT_CALL(*mockBroker_, getOrders("acc-001-sandbox"))
        .Times(1)
        .WillOnce(Return(std::vector<domain::Order>{}));

    auto req = createRequest("GET", "/api/v1/orders?account_id=acc-001-sandbox");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 0);
}

TEST_F(GetOrdersHandlerTest, AccountNotFound_Returns404) {
    EXPECT_CALL(*mockBroker_, getOrders("unknown-account"))
        .Times(1)
        .WillOnce(Throw(std::runtime_error("Account not found")));

    auto req = createRequest("GET", "/api/v1/orders?account_id=unknown-account");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["error"], "Account not found");
}

TEST_F(GetOrdersHandlerTest, MissingAccountId_Returns400) {
    auto req = createRequest("GET", "/api/v1/orders");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["error"].get<std::string>().find("account_id") != std::string::npos);
}

TEST_F(GetOrdersHandlerTest, PostMethod_Returns405) {
    auto req = createRequest("POST", "/api/v1/orders");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(GetOrdersHandlerTest, DeleteMethod_Returns405) {
    auto req = createRequest("DELETE", "/api/v1/orders");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 405);
}
