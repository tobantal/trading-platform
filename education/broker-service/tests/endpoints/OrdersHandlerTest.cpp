/**
 * @file OrdersHandlerTest.cpp
 * @brief Unit-тесты для OrdersHandler
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/OrdersHandler.hpp"
#include "ports/output/IBrokerGateway.hpp"

#include <IRequest.hpp>
#include <IResponse.hpp>
#include <nlohmann/json.hpp>

using namespace broker;
using namespace broker::adapters::primary;
using ::testing::Return;
using ::testing::Throw;
using ::testing::_;


// ============================================================================
// TestRequest - ведёт себя как BeastRequestAdapter
// ============================================================================

class TestRequest : public IRequest {
public:
    TestRequest(const std::string& method, const std::string& fullPath,
                const std::string& body = "")
        : method_(method), body_(body)
    {
        auto pos = fullPath.find('?');
        if (pos == std::string::npos) {
            path_ = fullPath;
        } else {
            path_ = fullPath.substr(0, pos);
            parseQueryString(fullPath.substr(pos + 1));
        }
    }

    std::string getPath() const override { return path_; }
    std::string getMethod() const override { return method_; }
    std::string getBody() const override { return body_; }
    std::string getIp() const override { return "127.0.0.1"; }
    int getPort() const override { return 8080; }
    std::map<std::string, std::string> getParams() const override { return params_; }
    std::map<std::string, std::string> getHeaders() const override { return headers_; }

    void setHeader(const std::string& name, const std::string& value) {
        headers_[name] = value;
    }

private:
    void parseQueryString(const std::string& query) {
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
                params_[key] = value;
            }

            if (amp == std::string::npos) break;
            start = amp + 1;
        }
    }

    std::string method_;
    std::string path_;
    std::string body_;
    std::map<std::string, std::string> params_;
    std::map<std::string, std::string> headers_;
};


// ============================================================================
// TestResponse
// ============================================================================

class TestResponse : public IResponse {
public:
    void setStatus(int code) override { status_ = code; }
    void setBody(const std::string& body) override { body_ = body; }
    void setHeader(const std::string& name, const std::string& value) override {
        headers_[name] = value;
    }

    int getStatus() const { return status_; }
    std::string getBody() const { return body_; }
    std::map<std::string, std::string> getHeaders() const { return headers_; }

private:
    int status_ = 0;
    std::string body_;
    std::map<std::string, std::string> headers_;
};


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

class OrdersHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockBroker_ = std::make_shared<MockBrokerGateway>();
        handler_ = std::make_unique<OrdersHandler>(mockBroker_);
    }

    nlohmann::json parseJson(const std::string& body) {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockBrokerGateway> mockBroker_;
    std::unique_ptr<OrdersHandler> handler_;
};


// ============================================================================
// ТЕСТЫ: POST /api/v1/orders
// ============================================================================

TEST_F(OrdersHandlerTest, PlaceOrder_SandboxAccount_Returns201) {
    domain::OrderResult result;
    result.orderId = "ord-12345";
    result.status = domain::OrderStatus::FILLED;
    result.executedLots = 10;
    result.executedPrice = domain::Money::fromDouble(265.0, "RUB");
    
    EXPECT_CALL(*mockBroker_, placeOrder(_, _))
        .Times(1)
        .WillOnce(Return(result));

    std::string body = R"({
        "account_id": "acc-001-sandbox",
        "figi": "BBG004730N88",
        "quantity": 10,
        "direction": "BUY",
        "order_type": "MARKET"
    })";

    TestRequest req("POST", "/api/v1/orders", body);
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 201);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["order_id"], "ord-12345");
    EXPECT_EQ(json["status"], "FILLED");
}

TEST_F(OrdersHandlerTest, PlaceOrder_LimitOrder_WithPrice) {
    domain::OrderResult result;
    result.orderId = "ord-67890";
    result.status = domain::OrderStatus::PENDING;
    result.executedLots = 0;
    
    EXPECT_CALL(*mockBroker_, placeOrder(_, _))
        .Times(1)
        .WillOnce(Return(result));

    std::string body = R"({
        "account_id": "acc-001-sandbox",
        "figi": "BBG004730N88",
        "quantity": 5,
        "direction": "BUY",
        "order_type": "LIMIT",
        "price": {"units": 260, "nano": 0, "currency": "RUB"}
    })";

    TestRequest req("POST", "/api/v1/orders", body);
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 201);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["order_id"], "ord-67890");
}

TEST_F(OrdersHandlerTest, PlaceOrder_NonSandboxAccount_ReturnsRejected) {
    domain::OrderResult result;
    result.orderId = "";
    result.status = domain::OrderStatus::REJECTED;
    result.message = "Account not found";
    
    EXPECT_CALL(*mockBroker_, placeOrder(_, _))
        .Times(1)
        .WillOnce(Return(result));

    std::string body = R"({
        "account_id": "real-production-account",
        "figi": "BBG004730N88",
        "quantity": 10,
        "direction": "BUY",
        "order_type": "MARKET"
    })";

    TestRequest req("POST", "/api/v1/orders", body);
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 201);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["status"], "REJECTED");
}

TEST_F(OrdersHandlerTest, PlaceOrder_MissingFields_Returns400) {
    std::string body = R"({
        "account_id": "acc-001-sandbox"
    })";

    TestRequest req("POST", "/api/v1/orders", body);
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
}

TEST_F(OrdersHandlerTest, PlaceOrder_InvalidJson_Returns400) {
    TestRequest req("POST", "/api/v1/orders", "not valid json");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
}


// ============================================================================
// ТЕСТЫ: GET /api/v1/orders
// ============================================================================

TEST_F(OrdersHandlerTest, GetOrders_ReturnsArray) {
    std::vector<domain::Order> orders;
    domain::Order order;
    order.id = "ord-111";  // domain::Order использует id, не orderId!
    order.accountId = "acc-001-sandbox";
    order.figi = "BBG004730N88";
    order.direction = domain::OrderDirection::BUY;
    order.type = domain::OrderType::MARKET;
    order.quantity = 10;
    order.status = domain::OrderStatus::FILLED;
    orders.push_back(order);
    
    EXPECT_CALL(*mockBroker_, getOrders("acc-001-sandbox"))
        .Times(1)
        .WillOnce(Return(orders));

    TestRequest req("GET", "/api/v1/orders?account_id=acc-001-sandbox");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 1);
    EXPECT_EQ(json[0]["order_id"], "ord-111");
}

TEST_F(OrdersHandlerTest, GetOrders_NonSandboxAccount_Returns404) {
    EXPECT_CALL(*mockBroker_, getOrders("unknown-real-account"))
        .Times(1)
        .WillOnce(Throw(std::runtime_error("Account not found")));

    TestRequest req("GET", "/api/v1/orders?account_id=unknown-real-account");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
}

TEST_F(OrdersHandlerTest, GetOrders_MissingAccountId_Returns400) {
    TestRequest req("GET", "/api/v1/orders");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
}


// ============================================================================
// ТЕСТЫ: GET /api/v1/orders/{id}
// ============================================================================

TEST_F(OrdersHandlerTest, GetOrderById_Found_Returns200) {
    domain::Order order;
    order.id = "ord-12345";  // domain::Order использует id!
    order.accountId = "acc-001-sandbox";
    order.figi = "BBG004730N88";
    order.status = domain::OrderStatus::FILLED;
    
    EXPECT_CALL(*mockBroker_, getOrderStatus("acc-001-sandbox", "ord-12345"))
        .Times(1)
        .WillOnce(Return(order));

    TestRequest req("GET", "/api/v1/orders/ord-12345?account_id=acc-001-sandbox");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["order_id"], "ord-12345");
}

TEST_F(OrdersHandlerTest, GetOrderById_NotFound_Returns404) {
    EXPECT_CALL(*mockBroker_, getOrderStatus(_, _))
        .Times(1)
        .WillOnce(Return(std::nullopt));

    TestRequest req("GET", "/api/v1/orders/non-existent?account_id=acc-001-sandbox");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
}


// ============================================================================
// ТЕСТЫ: DELETE /api/v1/orders/{id}
// ============================================================================

TEST_F(OrdersHandlerTest, CancelOrder_Success_Returns200) {
    EXPECT_CALL(*mockBroker_, cancelOrder("acc-001-sandbox", "ord-12345"))
        .Times(1)
        .WillOnce(Return(true));

    TestRequest req("DELETE", "/api/v1/orders/ord-12345?account_id=acc-001-sandbox");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["order_id"], "ord-12345");
    EXPECT_EQ(json["status"], "CANCELLED");
}

TEST_F(OrdersHandlerTest, CancelOrder_NotFound_Returns404) {
    EXPECT_CALL(*mockBroker_, cancelOrder(_, _))
        .Times(1)
        .WillOnce(Return(false));

    TestRequest req("DELETE", "/api/v1/orders/non-existent?account_id=acc-001-sandbox");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
}


// ============================================================================
// ТЕСТЫ: HTTP методы
// ============================================================================

TEST_F(OrdersHandlerTest, PutMethod_Returns405) {
    TestRequest req("PUT", "/api/v1/orders/ord-12345");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 405);
}
