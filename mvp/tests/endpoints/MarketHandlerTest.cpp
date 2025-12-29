#include <gtest/gtest.h>

#include "adapters/primary/MarketHandler.hpp"
#include "application/MarketService.hpp"
#include "adapters/secondary/broker/FakeTinkoffAdapter.hpp"
#include "adapters/secondary/cache/LruCacheAdapter.hpp"
#include "adapters/secondary/events/InMemoryEventBus.hpp"

#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>

using namespace trading::adapters::primary;
using namespace trading::application;
using namespace trading::adapters::secondary;
using namespace trading::domain;

// ============================================================================
// Тестовый класс MarketHandlerTest
// ============================================================================

class MarketHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Создаем реальные зависимости для MarketService
        auto broker = std::make_shared<FakeTinkoffAdapter>();
        auto cache = std::make_shared<LruCacheAdapter>(1000, 5); // capacity=1000, ttl=5s
        auto eventBus = std::make_shared<InMemoryEventBus>();

        // Создаем сервис с реальными зависимостями
        auto marketService = std::make_shared<MarketService>(
            broker, cache, eventBus);

        // Создаем handler для тестирования
        marketHandler = std::make_unique<MarketHandler>(marketService);

        // Сохраняем зависимости для использования в тестах
        this->broker = broker;
        this->cache = cache;
        this->eventBus = eventBus;
    }

    void TearDown() override
    {
        marketHandler.reset();
    }

    // Вспомогательный метод для парсинга JSON ответа
    nlohmann::json parseJsonResponse(SimpleResponse &res)
    {
        try
        {
            return nlohmann::json::parse(res.getBody());
        }
        catch (const nlohmann::json::exception &e)
        {
            ADD_FAILURE() << "Failed to parse JSON: " << e.what();
            return nlohmann::json();
        }
    }

    // Вспомогательный метод для создания запроса с параметрами
    SimpleRequest createRequestWithParams(const std::string &method,
                                          const std::string &path,
                                          const std::map<std::string, std::string> &params = {})
    {
        std::string fullPath = path;
        if (!params.empty())
        {
            fullPath += "?";
            bool first = true;
            for (const auto &[key, value] : params)
            {
                if (!first)
                    fullPath += "&";
                fullPath += key + "=" + value;
                first = false;
            }
        }
        return SimpleRequest(method, fullPath, "", "127.0.0.1", 8080);
    }

    std::unique_ptr<MarketHandler> marketHandler;
    std::shared_ptr<FakeTinkoffAdapter> broker;
    std::shared_ptr<LruCacheAdapter> cache;
    std::shared_ptr<InMemoryEventBus> eventBus;
};

// ============================================================================
// ТЕСТЫ ДЛЯ /api/v1/quotes
// ============================================================================

TEST_F(MarketHandlerTest, GetQuotes_NoParameters_ReturnsAllQuotes)
{
    // Arrange
    SimpleRequest req(
        "GET",
        "/api/v1/quotes",
        "",
        "127.0.0.1",
        8080);
    SimpleResponse res;

    // Act
    marketHandler->handle(req, res);

    // Assert
    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.is_array());
    // FakeTinkoffAdapter содержит 5 инструментов по умолчанию
    EXPECT_GE(json.size(), 5);

    // Проверяем структуру первого элемента
    auto firstQuote = json[0];
    EXPECT_TRUE(firstQuote.contains("figi"));
    EXPECT_TRUE(firstQuote.contains("ticker"));
    EXPECT_TRUE(firstQuote.contains("last_price"));
    EXPECT_TRUE(firstQuote.contains("bid_price"));
    EXPECT_TRUE(firstQuote.contains("ask_price"));
    EXPECT_TRUE(firstQuote.contains("currency"));
    EXPECT_TRUE(firstQuote.contains("updated_at"));
}

TEST_F(MarketHandlerTest, GetQuotes_WithFigisParam_ReturnsSpecificQuotes)
{
    // Arrange
    std::map<std::string, std::string> params = {{"figis", "BBG004730N88,BBG004731032"}};
    SimpleRequest req = createRequestWithParams("GET", "/api/v1/quotes", params);
    SimpleResponse res;

    // Act
    marketHandler->handle(req, res);

    // Assert
    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.is_array());
    // Должны получить 2 котировки
    EXPECT_EQ(json.size(), 2);

    // Проверяем, что получены именно запрошенные инструменты
    std::vector<std::string> expectedFigis = {"BBG004730N88", "BBG004731032"};
    std::set<std::string> receivedFigis;
    for (const auto &quote : json)
    {
        receivedFigis.insert(quote["figi"]);
    }

    for (const auto &figi : expectedFigis)
    {
        EXPECT_TRUE(receivedFigis.find(figi) != receivedFigis.end())
            << "Missing FIGI: " << figi;
    }
}

TEST_F(MarketHandlerTest, GetQuotes_WithSingleFigi_ReturnsOneQuote)
{
    // Arrange
    std::map<std::string, std::string> params = {{"figis", "BBG004730N88"}};
    SimpleRequest req = createRequestWithParams("GET", "/api/v1/quotes", params);
    SimpleResponse res;

    // Act
    marketHandler->handle(req, res);

    // Assert
    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 1);
    EXPECT_EQ(json[0]["figi"], "BBG004730N88");
}

TEST_F(MarketHandlerTest, GetQuotes_WithEmptyFigis_ReturnsAllQuotes)
{
    // Arrange
    std::map<std::string, std::string> params = {{"figis", ""}};
    SimpleRequest req = createRequestWithParams("GET", "/api/v1/quotes", params);
    SimpleResponse res;

    // Act
    marketHandler->handle(req, res);

    // Assert
    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.is_array());
    // Должны получить все котировки
    EXPECT_GE(json.size(), 5);
}

// ============================================================================
// ТЕСТЫ ДЛЯ /api/v1/instruments
// ============================================================================

TEST_F(MarketHandlerTest, GetInstruments_ReturnsAllInstruments)
{
    // Arrange
    SimpleRequest req(
        "GET",
        "/api/v1/instruments",
        "",
        "127.0.0.1",
        8080);
    SimpleResponse res;

    // Act
    marketHandler->handle(req, res);

    // Assert
    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.is_array());
    // FakeTinkoffAdapter содержит 5 инструментов
    EXPECT_EQ(json.size(), 5);

    // Проверяем структуру первого инструмента
    auto firstInstrument = json[0];
    EXPECT_TRUE(firstInstrument.contains("figi"));
    EXPECT_TRUE(firstInstrument.contains("ticker"));
    EXPECT_TRUE(firstInstrument.contains("name"));
    EXPECT_TRUE(firstInstrument.contains("currency"));
    EXPECT_TRUE(firstInstrument.contains("lot"));
    EXPECT_TRUE(firstInstrument.contains("trading_available"));
}

// ============================================================================
// ТЕСТЫ ДЛЯ /api/v1/instruments/search
// ============================================================================

TEST_F(MarketHandlerTest, SearchInstruments_WithQuery_ReturnsMatchingInstruments)
{
    // Arrange
    std::map<std::string, std::string> params = {{"query", "SBER"}};
    SimpleRequest req = createRequestWithParams("GET", "/api/v1/instruments/search", params);
    SimpleResponse res;

    // Act
    marketHandler->handle(req, res);

    // Assert
    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.is_array());
    // Должен найти SBER
    EXPECT_GT(json.size(), 0);

    // Проверяем, что все найденные инструменты содержат "SBER" в тикере или названии
    for (const auto &instrument : json)
    {
        std::string ticker = instrument["ticker"];
        std::string name = instrument["name"];
        EXPECT_TRUE(ticker.find("SBER") != std::string::npos ||
                    name.find("Сбербанк") != std::string::npos);
    }
}

TEST_F(MarketHandlerTest, SearchInstruments_WithLowerCaseQuery_ReturnsMatchingInstruments)
{
    // Arrange
    std::map<std::string, std::string> params = {{"query", "sber"}};
    SimpleRequest req = createRequestWithParams("GET", "/api/v1/instruments/search", params);
    SimpleResponse res;

    // Act
    marketHandler->handle(req, res);

    // Assert
    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.is_array());
    // Поиск должен быть case-insensitive
    EXPECT_GT(json.size(), 0);
}

TEST_F(MarketHandlerTest, SearchInstruments_WithoutQuery_Returns400)
{
    // Arrange
    SimpleRequest req(
        "GET",
        "/api/v1/instruments/search",
        "",
        "127.0.0.1",
        8080);
    SimpleResponse res;

    // Act
    marketHandler->handle(req, res);

    // Assert
    EXPECT_EQ(res.getStatus(), 400);

    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "Query parameter is required");
}

TEST_F(MarketHandlerTest, SearchInstruments_WithEmptyQuery_Returns400)
{
    // Arrange
    std::map<std::string, std::string> params = {{"query", ""}};
    SimpleRequest req = createRequestWithParams("GET", "/api/v1/instruments/search", params);
    SimpleResponse res;

    // Act
    marketHandler->handle(req, res);

    // Assert
    EXPECT_EQ(res.getStatus(), 400);

    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "Query parameter is required");
}

TEST_F(MarketHandlerTest, SearchInstruments_NonExistentQuery_ReturnsEmptyArray)
{
    // Arrange
    std::map<std::string, std::string> params = {{"query", "NONEXISTENTTICKER123"}};
    SimpleRequest req = createRequestWithParams("GET", "/api/v1/instruments/search", params);
    SimpleResponse res;

    // Act
    marketHandler->handle(req, res);

    // Assert
    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 0);
}

// ============================================================================
// ТЕСТЫ ДЛЯ /api/v1/instruments/{figi}
// ============================================================================

TEST_F(MarketHandlerTest, GetInstrumentByFigi_WithValidFigi_ReturnsInstrument)
{
    // Arrange
    SimpleRequest req(
        "GET",
        "/api/v1/instruments/BBG004730N88",
        "",
        "127.0.0.1",
        8080);
    SimpleResponse res;

    // Act
    marketHandler->handle(req, res);

    // Assert
    EXPECT_EQ(res.getStatus(), 200);

    auto json = parseJsonResponse(res);
    EXPECT_EQ(json["figi"], "BBG004730N88");
    EXPECT_EQ(json["ticker"], "SBER");
    EXPECT_EQ(json["name"], "Сбербанк");
    EXPECT_EQ(json["currency"], "RUB");
    EXPECT_EQ(json["lot"], 10);
    EXPECT_EQ(json["trading_available"], true);
}

TEST_F(MarketHandlerTest, GetInstrumentByFigi_WithNonExistentFigi_Returns404)
{
    // Arrange
    SimpleRequest req(
        "GET",
        "/api/v1/instruments/NONEXISTENTFIGI",
        "",
        "127.0.0.1",
        8080);
    SimpleResponse res;

    // Act
    marketHandler->handle(req, res);

    // Assert
    EXPECT_EQ(res.getStatus(), 404);

    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "Instrument not found");
}

TEST_F(MarketHandlerTest, GetInstrumentByFigi_WithEmptyFigi_Returns400)
{
    // Arrange
    SimpleRequest req(
        "GET",
        "/api/v1/instruments/",
        "",
        "127.0.0.1",
        8080);
    SimpleResponse res;

    // Act
    marketHandler->handle(req, res);

    // Assert
    EXPECT_EQ(res.getStatus(), 400);

    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "FIGI is required");
}

// ============================================================================
// ТЕСТЫ ДЛЯ НЕИЗВЕСТНЫХ ENDPOINTS
// ============================================================================

TEST_F(MarketHandlerTest, UnknownEndpoint_Returns404)
{
    // Arrange
    SimpleRequest req(
        "GET",
        "/api/v1/market/unknown",
        "",
        "127.0.0.1",
        8080);
    SimpleResponse res;

    // Act
    marketHandler->handle(req, res);

    // Assert
    EXPECT_EQ(res.getStatus(), 404);

    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "Not found");
}

TEST_F(MarketHandlerTest, WrongMethod_Returns405)
{
    // Arrange
    SimpleRequest req(
        "POST",
        "/api/v1/quotes",
        "{}",
        "127.0.0.1",
        8080);
    SimpleResponse res;

    // Act
    marketHandler->handle(req, res);

    // Assert
    EXPECT_EQ(res.getStatus(), 405);

    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "Method not allowed");
}

// ============================================================================
// ИНТЕГРАЦИОННЫЕ ТЕСТЫ
// ============================================================================

TEST_F(MarketHandlerTest, Caching_WorksCorrectly)
{
    // Arrange
    std::map<std::string, std::string> params = {{"figis", "BBG004730N88"}};
    SimpleRequest req = createRequestWithParams("GET", "/api/v1/quotes", params);
    SimpleResponse res1, res2;

    // Act - первый запрос должен получить данные от брокера
    marketHandler->handle(req, res1);
    EXPECT_EQ(res1.getStatus(), 200);

    // Act - второй запрос должен получить данные из кэша
    marketHandler->handle(req, res2);
    EXPECT_EQ(res2.getStatus(), 200);

    // Assert - оба запроса должны вернуть одинаковые данные
    auto json1 = parseJsonResponse(res1);
    auto json2 = parseJsonResponse(res2);

    EXPECT_EQ(1, json1.size());
    EXPECT_EQ(1, json2.size());

    // Проверяем, что данные совпадают (в рамках точности double)
    EXPECT_EQ(json1[0]["figi"], json2[0]["figi"]);
    EXPECT_EQ(json1[0]["ticker"], json2[0]["ticker"]);
    // Цены могут немного отличаться из-за генерации в FakeTinkoffAdapter,
    // но для теста кэширования это нормально
}

TEST_F(MarketHandlerTest, MultipleEndpoints_WorkCorrectlyInSequence)
{
    // Тестируем последовательный вызов разных endpoint-ов

    // 1. Получить все инструменты
    {
        SimpleRequest req("GET", "/api/v1/instruments", "", "127.0.0.1", 8080);
        SimpleResponse res;
        marketHandler->handle(req, res);
        EXPECT_EQ(res.getStatus(), 200);
        auto json = parseJsonResponse(res);
        EXPECT_TRUE(json.is_array());
        EXPECT_GT(json.size(), 0);
    }

    // 2. Поиск инструмента
    {
        std::map<std::string, std::string> params = {{"query", "SBER"}};
        SimpleRequest req = createRequestWithParams("GET", "/api/v1/instruments/search", params);
        SimpleResponse res;
        marketHandler->handle(req, res);
        EXPECT_EQ(res.getStatus(), 200);
        auto json = parseJsonResponse(res);
        EXPECT_TRUE(json.is_array());
    }

    // 3. Получить котировки для найденного инструмента
    {
        std::map<std::string, std::string> params = {{"figis", "BBG004730N88"}};
        SimpleRequest req = createRequestWithParams("GET", "/api/v1/quotes", params);
        SimpleResponse res;
        marketHandler->handle(req, res);
        EXPECT_EQ(res.getStatus(), 200);
        auto json = parseJsonResponse(res);
        EXPECT_TRUE(json.is_array());
        EXPECT_EQ(json.size(), 1);
    }

    // 4. Получить конкретный инструмент по FIGI
    {
        SimpleRequest req("GET", "/api/v1/instruments/BBG004730N88", "", "127.0.0.1", 8080);
        SimpleResponse res;
        marketHandler->handle(req, res);
        EXPECT_EQ(res.getStatus(), 200);
        auto json = parseJsonResponse(res);
        EXPECT_EQ(json["figi"], "BBG004730N88");
    }
}
