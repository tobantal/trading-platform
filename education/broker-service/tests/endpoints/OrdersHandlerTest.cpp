/**
 * @file OrdersHandlerTest.cpp
 * @brief Unit-тесты для OrdersHandler
 *
 * ВАЖНО: POST и DELETE теперь через RabbitMQ, не через HTTP!
 * OrdersHandler обрабатывает только GET запросы.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/OrdersHandler.hpp"
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

class OrdersHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockBroker_ = std::make_shared<MockBrokerGateway>();
        handler_ = std::make_unique<OrdersHandler>(mockBroker_);
    }

    /**
     * @brief Хелпер для создания запроса с парсингом query string
     * 
     * Позволяет использовать привычный синтаксис:
     *   createRequest("GET", "/api/v1/orders?account_id=acc-001")
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

    nlohmann::json parseJson(const std::string& body) {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockBrokerGateway> mockBroker_;
    std::unique_ptr<OrdersHandler> handler_;

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
// ТЕСТЫ: POST /api/v1/orders — теперь возвращает 405!
// ============================================================================

TEST_F(OrdersHandlerTest, PostOrder_Returns405_EventDriven) {
    std::string body = R"({
        "account_id": "acc-001-sandbox",
        "figi": "BBG004730N88",
        "quantity": 10,
        "direction": "BUY",
        "order_type": "MARKET"
    })";

    auto req = createRequest("POST", "/api/v1/orders", body);
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    // POST теперь через RabbitMQ, HTTP возвращает 405
    EXPECT_EQ(res.getStatus(), 405);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("error"));
}

// ============================================================================
// ТЕСТЫ: GET /api/v1/orders
// ============================================================================

TEST_F(OrdersHandlerTest, GetOrders_ReturnsArray) {
    std::vector<domain::Order> orders;
    domain::Order order;
    order.id = "ord-111";
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

    auto req = createRequest("GET", "/api/v1/orders?account_id=acc-001-sandbox");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 1);
    EXPECT_EQ(json[0]["order_id"], "ord-111");
}

TEST_F(OrdersHandlerTest, GetOrders_EmptyList_Returns200) {
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

TEST_F(OrdersHandlerTest, GetOrders_AccountNotFound_Returns404) {
    EXPECT_CALL(*mockBroker_, getOrders("unknown-account"))
        .Times(1)
        .WillOnce(Throw(std::runtime_error("Account not found")));

    auto req = createRequest("GET", "/api/v1/orders?account_id=unknown-account");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
}

TEST_F(OrdersHandlerTest, GetOrders_MissingAccountId_Returns400) {
    auto req = createRequest("GET", "/api/v1/orders");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
}

// ============================================================================
// ТЕСТЫ: GET /api/v1/orders/{id}
// ============================================================================

TEST_F(OrdersHandlerTest, GetOrderById_Found_Returns200) {
    domain::Order order;
    order.id = "ord-12345";
    order.accountId = "acc-001-sandbox";
    order.figi = "BBG004730N88";
    order.status = domain::OrderStatus::FILLED;
    
    EXPECT_CALL(*mockBroker_, getOrderStatus("acc-001-sandbox", "ord-12345"))
        .Times(1)
        .WillOnce(Return(order));

    auto req = createRequest("GET", "/api/v1/orders/ord-12345?account_id=acc-001-sandbox");
    req.setPathPattern("/api/v1/orders/*");
    
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["order_id"], "ord-12345");
    EXPECT_EQ(json["status"], "FILLED");
}

TEST_F(OrdersHandlerTest, GetOrderById_NotFound_Returns404) {
    EXPECT_CALL(*mockBroker_, getOrderStatus(_, _))
        .Times(1)
        .WillOnce(Return(std::nullopt));

    auto req = createRequest("GET", "/api/v1/orders/non-existent?account_id=acc-001-sandbox");
    req.setPathPattern("/api/v1/orders/*");
    
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
}

// ============================================================================
// ТЕСТЫ: DELETE /api/v1/orders/{id} — теперь возвращает 405!
// ============================================================================

TEST_F(OrdersHandlerTest, DeleteOrder_Returns405_EventDriven) {
    auto req = createRequest("DELETE", "/api/v1/orders/ord-12345?account_id=acc-001-sandbox");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    // DELETE теперь через RabbitMQ, HTTP возвращает 405
    EXPECT_EQ(res.getStatus(), 405);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("error"));
}

// ============================================================================
// ТЕСТЫ: Другие HTTP методы
// ============================================================================

TEST_F(OrdersHandlerTest, PutMethod_Returns405) {
    auto req = createRequest("PUT", "/api/v1/orders/ord-12345");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(OrdersHandlerTest, PatchMethod_Returns405) {
    auto req = createRequest("PATCH", "/api/v1/orders/ord-12345");
    SimpleResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 405);
}