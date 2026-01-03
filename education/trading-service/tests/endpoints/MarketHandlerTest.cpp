#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/primary/MarketHandler.hpp"
#include "ports/input/IMarketService.hpp"
#include "domain/Quote.hpp"
#include "domain/Instrument.hpp"

#include <IRequest.hpp>
#include <IResponse.hpp>
#include <nlohmann/json.hpp>

using namespace trading;
using namespace trading::adapters::primary;
using ::testing::Return;
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
        // Парсим path и params как BeastRequestAdapter
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
// Mock для IMarketService
// ============================================================================

class MockMarketService : public ports::input::IMarketService {
public:
    MOCK_METHOD(std::optional<domain::Quote>, getQuote, (const std::string& figi), (override));
    MOCK_METHOD(std::vector<domain::Quote>, getQuotes, (const std::vector<std::string>& figis), (override));
    MOCK_METHOD(std::vector<domain::Instrument>, searchInstruments, (const std::string& query), (override));
    MOCK_METHOD(std::optional<domain::Instrument>, getInstrumentByFigi, (const std::string& figi), (override));
    MOCK_METHOD(std::vector<domain::Instrument>, getAllInstruments, (), (override));
};

// ============================================================================
// Тестовый класс
// ============================================================================

class MarketHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockService_ = std::make_shared<MockMarketService>();
        handler_ = std::make_unique<MarketHandler>(mockService_);
    }

    // Хелперы для создания тестовых данных
    domain::Quote createQuote(const std::string& figi, const std::string& ticker, double price) {
        return domain::Quote(
            figi,
            ticker,
            domain::Money::fromDouble(price, "RUB"),
            domain::Money::fromDouble(price - 0.5, "RUB"),
            domain::Money::fromDouble(price + 0.5, "RUB")
        );
    }

    domain::Instrument createInstrument(const std::string& figi, const std::string& ticker, 
                                         const std::string& name) {
        return domain::Instrument(figi, ticker, name, "RUB", 10);
    }

    nlohmann::json parseJson(const std::string& body) {
        return nlohmann::json::parse(body);
    }

    std::shared_ptr<MockMarketService> mockService_;
    std::unique_ptr<MarketHandler> handler_;
};

// ============================================================================
// ТЕСТЫ: GET /api/v1/instruments
// ============================================================================

TEST_F(MarketHandlerTest, GetAllInstruments_ReturnsInstrumentsList) {
    std::vector<domain::Instrument> instruments = {
        createInstrument("BBG004730N88", "SBER", "Сбербанк"),
        createInstrument("BBG004730RP0", "GAZP", "Газпром")
    };
    
    EXPECT_CALL(*mockService_, getAllInstruments())
        .Times(1)
        .WillOnce(Return(instruments));

    TestRequest req("GET", "/api/v1/instruments");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("instruments"));
    EXPECT_EQ(json["instruments"].size(), 2);
    EXPECT_EQ(json["instruments"][0]["figi"], "BBG004730N88");
    EXPECT_EQ(json["instruments"][0]["ticker"], "SBER");
    EXPECT_EQ(json["instruments"][1]["figi"], "BBG004730RP0");
}

TEST_F(MarketHandlerTest, GetAllInstruments_EmptyList_ReturnsEmptyArray) {
    EXPECT_CALL(*mockService_, getAllInstruments())
        .Times(1)
        .WillOnce(Return(std::vector<domain::Instrument>{}));

    TestRequest req("GET", "/api/v1/instruments");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json["instruments"].is_array());
    EXPECT_EQ(json["instruments"].size(), 0);
}

// ============================================================================
// ТЕСТЫ: GET /api/v1/instruments/{figi}
// ============================================================================

TEST_F(MarketHandlerTest, GetInstrumentByFigi_Found_ReturnsInstrument) {
    auto sber = createInstrument("BBG004730N88", "SBER", "Сбербанк");
    
    EXPECT_CALL(*mockService_, getInstrumentByFigi("BBG004730N88"))
        .Times(1)
        .WillOnce(Return(sber));

    TestRequest req("GET", "/api/v1/instruments/BBG004730N88");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["figi"], "BBG004730N88");
    EXPECT_EQ(json["ticker"], "SBER");
    EXPECT_EQ(json["name"], "Сбербанк");
    EXPECT_EQ(json["currency"], "RUB");
    EXPECT_EQ(json["lot"], 10);
}

TEST_F(MarketHandlerTest, GetInstrumentByFigi_NotFound_Returns404) {
    EXPECT_CALL(*mockService_, getInstrumentByFigi("UNKNOWN"))
        .Times(1)
        .WillOnce(Return(std::nullopt));

    TestRequest req("GET", "/api/v1/instruments/UNKNOWN");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("error"));
}

// ============================================================================
// ТЕСТЫ: GET /api/v1/instruments/search?query=...
// ============================================================================

TEST_F(MarketHandlerTest, SearchInstruments_WithQuery_ReturnsMatches) {
    std::vector<domain::Instrument> results = {
        createInstrument("BBG004730N88", "SBER", "Сбербанк")
    };
    
    EXPECT_CALL(*mockService_, searchInstruments("SBER"))
        .Times(1)
        .WillOnce(Return(results));

    TestRequest req("GET", "/api/v1/instruments/search?query=SBER");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("instruments"));
    EXPECT_EQ(json["instruments"].size(), 1);
    EXPECT_EQ(json["instruments"][0]["ticker"], "SBER");
}

TEST_F(MarketHandlerTest, SearchInstruments_NoQueryParam_Returns400) {
    EXPECT_CALL(*mockService_, searchInstruments(_))
        .Times(0);

    TestRequest req("GET", "/api/v1/instruments/search");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("error"));
    EXPECT_TRUE(json["error"].get<std::string>().find("query") != std::string::npos);
}

TEST_F(MarketHandlerTest, SearchInstruments_EmptyQuery_Returns400) {
    EXPECT_CALL(*mockService_, searchInstruments(_))
        .Times(0);

    TestRequest req("GET", "/api/v1/instruments/search?query=");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
}

TEST_F(MarketHandlerTest, SearchInstruments_NoMatches_ReturnsEmptyArray) {
    EXPECT_CALL(*mockService_, searchInstruments("NONEXISTENT"))
        .Times(1)
        .WillOnce(Return(std::vector<domain::Instrument>{}));

    TestRequest req("GET", "/api/v1/instruments/search?query=NONEXISTENT");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["instruments"].size(), 0);
}

// ============================================================================
// ТЕСТЫ: GET /api/v1/quotes?figis=...
// ============================================================================

TEST_F(MarketHandlerTest, GetQuotes_WithFigis_ReturnsQuotes) {
    std::vector<domain::Quote> quotes = {
        createQuote("BBG004730N88", "SBER", 280.0),
        createQuote("BBG004730RP0", "GAZP", 150.0)
    };
    
    EXPECT_CALL(*mockService_, getQuotes(std::vector<std::string>{"BBG004730N88", "BBG004730RP0"}))
        .Times(1)
        .WillOnce(Return(quotes));

    TestRequest req("GET", "/api/v1/quotes?figis=BBG004730N88,BBG004730RP0");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("quotes"));
    EXPECT_EQ(json["quotes"].size(), 2);
    EXPECT_EQ(json["quotes"][0]["figi"], "BBG004730N88");
    EXPECT_EQ(json["quotes"][1]["figi"], "BBG004730RP0");
}

TEST_F(MarketHandlerTest, GetQuotes_SingleFigi_ReturnsOneQuote) {
    std::vector<domain::Quote> quotes = {
        createQuote("BBG004730N88", "SBER", 280.0)
    };
    
    EXPECT_CALL(*mockService_, getQuotes(std::vector<std::string>{"BBG004730N88"}))
        .Times(1)
        .WillOnce(Return(quotes));

    TestRequest req("GET", "/api/v1/quotes?figis=BBG004730N88");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJson(res.getBody());
    EXPECT_EQ(json["quotes"].size(), 1);
}

TEST_F(MarketHandlerTest, GetQuotes_NoFigisParam_Returns400) {
    EXPECT_CALL(*mockService_, getQuotes(_))
        .Times(0);

    TestRequest req("GET", "/api/v1/quotes");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJson(res.getBody());
    EXPECT_TRUE(json.contains("error"));
    EXPECT_TRUE(json["error"].get<std::string>().find("figis") != std::string::npos);
}

TEST_F(MarketHandlerTest, GetQuotes_EmptyFigis_Returns400) {
    EXPECT_CALL(*mockService_, getQuotes(_))
        .Times(0);

    TestRequest req("GET", "/api/v1/quotes?figis=");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 400);
}

// ============================================================================
// ТЕСТЫ: Проверка парсинга query params
// ============================================================================

TEST_F(MarketHandlerTest, QueryParams_ParsedCorrectly) {
    std::vector<domain::Quote> quotes = {createQuote("TEST", "TEST", 100.0)};
    
    EXPECT_CALL(*mockService_, getQuotes(std::vector<std::string>{"TEST"}))
        .Times(1)
        .WillOnce(Return(quotes));

    TestRequest req("GET", "/api/v1/quotes?figis=TEST");
    TestResponse res;
    
    // Проверим что getPath() НЕ содержит query string (как BeastRequestAdapter)
    EXPECT_EQ(req.getPath(), "/api/v1/quotes");
    
    // Проверим что getParams() содержит параметры
    auto params = req.getParams();
    EXPECT_EQ(params["figis"], "TEST");
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
}

TEST_F(MarketHandlerTest, QueryParams_MultipleParams_ParsedCorrectly) {
    std::vector<domain::Instrument> results = {
        createInstrument("BBG004730N88", "SBER", "Сбербанк")
    };
    
    EXPECT_CALL(*mockService_, searchInstruments("SBER"))
        .Times(1)
        .WillOnce(Return(results));

    // URL с несколькими параметрами
    TestRequest req("GET", "/api/v1/instruments/search?query=SBER&limit=10");
    TestResponse res;
    
    // Проверяем парсинг
    auto params = req.getParams();
    EXPECT_EQ(params["query"], "SBER");
    EXPECT_EQ(params["limit"], "10");
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 200);
}

// ============================================================================
// ТЕСТЫ: HTTP методы
// ============================================================================

TEST_F(MarketHandlerTest, PostMethod_Returns405) {
    TestRequest req("POST", "/api/v1/instruments");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(MarketHandlerTest, PutMethod_Returns405) {
    TestRequest req("PUT", "/api/v1/instruments/BBG004730N88");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 405);
}

TEST_F(MarketHandlerTest, DeleteMethod_Returns405) {
    TestRequest req("DELETE", "/api/v1/instruments/BBG004730N88");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 405);
}

// ============================================================================
// ТЕСТЫ: Неизвестные пути
// ============================================================================

TEST_F(MarketHandlerTest, UnknownPath_Returns404) {
    TestRequest req("GET", "/api/v1/unknown");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
}

TEST_F(MarketHandlerTest, RootPath_Returns404) {
    TestRequest req("GET", "/");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
}

// ============================================================================
// ТЕСТЫ: Edge cases для instruments/{figi}
// ============================================================================

TEST_F(MarketHandlerTest, InstrumentsPath_TrailingSlash_Returns404) {
    // /api/v1/instruments/ без FIGI
    TestRequest req("GET", "/api/v1/instruments/");
    TestResponse res;
    
    handler_->handle(req, res);
    
    EXPECT_EQ(res.getStatus(), 404);
}

TEST_F(MarketHandlerTest, InstrumentsSearch_NotConfusedWithFigi) {
    // /api/v1/instruments/search - это search, не FIGI="search"
    EXPECT_CALL(*mockService_, searchInstruments(_))
        .Times(0);
    EXPECT_CALL(*mockService_, getInstrumentByFigi(_))
        .Times(0);

    TestRequest req("GET", "/api/v1/instruments/search");
    TestResponse res;
    
    handler_->handle(req, res);
    
    // Должен вернуть 400 (нет query param), а не 404 (instrument not found)
    EXPECT_EQ(res.getStatus(), 400);
}
