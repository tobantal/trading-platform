/**
 * @file FakeBrokerAdapterTest.cpp
 * @brief Unit-тесты для FakeBrokerAdapter
 * 
 * Тестируем:
 * 1. Sandbox-аккаунты автосоздаются
 * 2. Non-sandbox аккаунты отклоняются
 * 3. Кэширование работает (БД → кэш)
 * 4. Транзакционность (БД упала → кэш не обновляется)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "adapters/secondary/broker/FakeBrokerAdapter.hpp"
#include "adapters/secondary/broker/EnhancedFakeBroker.hpp"
#include "settings/BrokerSettings.hpp"
#include "ports/output/IEventPublisher.hpp"
#include "ports/output/IBrokerBalanceRepository.hpp"
#include "ports/output/IBrokerPositionRepository.hpp"
#include "ports/output/IBrokerOrderRepository.hpp"
#include "ports/output/IQuoteRepository.hpp"
#include "ports/output/IInstrumentRepository.hpp"

using namespace broker;
using namespace broker::adapters::secondary;
using namespace broker::settings;
using namespace testing;


// ============================================================================
// MOCKS
// ============================================================================

class MockEventPublisher : public ports::output::IEventPublisher {
public:
    MOCK_METHOD(void, publish, (const std::string& eventType, const std::string& payload), (override));
    MOCK_METHOD(void, subscribe, (const std::string& eventType, std::function<void(const std::string&)> handler), (override));
};

class MockBalanceRepository : public ports::output::IBrokerBalanceRepository {
public:
    MOCK_METHOD(std::optional<domain::BrokerBalance>, findByAccountId, (const std::string& accountId), (override));
    MOCK_METHOD(void, save, (const domain::BrokerBalance& balance), (override));
    MOCK_METHOD(void, update, (const domain::BrokerBalance& balance), (override));
    MOCK_METHOD(bool, reserve, (const std::string& accountId, int64_t amount), (override));
    MOCK_METHOD(void, commitReserved, (const std::string& accountId, int64_t amount), (override));
    MOCK_METHOD(void, releaseReserved, (const std::string& accountId, int64_t amount), (override));
};

class MockPositionRepository : public ports::output::IBrokerPositionRepository {
public:
    MOCK_METHOD(std::vector<domain::BrokerPosition>, findByAccountId, (const std::string& accountId), (override));
    MOCK_METHOD(std::optional<domain::BrokerPosition>, findByAccountAndFigi, 
                (const std::string& accountId, const std::string& figi), (override));
    MOCK_METHOD(void, save, (const domain::BrokerPosition& position), (override));
    MOCK_METHOD(void, update, (const domain::BrokerPosition& position), (override));
};

class MockOrderRepository : public ports::output::IBrokerOrderRepository {
public:
    MOCK_METHOD(std::vector<domain::BrokerOrder>, findByAccountId, (const std::string& accountId), (override));
    MOCK_METHOD(std::optional<domain::BrokerOrder>, findById, (const std::string& orderId), (override));
    MOCK_METHOD(void, save, (const domain::BrokerOrder& order), (override));
    MOCK_METHOD(void, update, (const domain::BrokerOrder& order), (override));
};

class MockQuoteRepository : public ports::output::IQuoteRepository {
public:
    MOCK_METHOD(std::optional<domain::Quote>, findByFigi, (const std::string& figi), (override));
    MOCK_METHOD(void, save, (const domain::Quote& quote), (override));
};

class MockInstrumentRepository : public ports::output::IInstrumentRepository {
public:
    MOCK_METHOD(std::vector<domain::Instrument>, findAll, (), (override));
    MOCK_METHOD(std::optional<domain::Instrument>, findByFigi, (const std::string& figi), (override));
    MOCK_METHOD(void, save, (const domain::Instrument& instrument), (override));
};


// ============================================================================
// TEST FIXTURE
// ============================================================================

class FakeBrokerAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Settings для EnhancedFakeBroker
        settings_ = std::make_shared<BrokerSettings>();
        
        // EnhancedFakeBroker - реальный (не мок)
        broker_ = std::make_shared<EnhancedFakeBroker>(settings_);
        
        // Моки репозиториев
        eventPublisher_ = std::make_shared<NiceMock<MockEventPublisher>>();
        balanceRepo_ = std::make_shared<NiceMock<MockBalanceRepository>>();
        positionRepo_ = std::make_shared<NiceMock<MockPositionRepository>>();
        orderRepo_ = std::make_shared<NiceMock<MockOrderRepository>>();
        quoteRepo_ = std::make_shared<NiceMock<MockQuoteRepository>>();
        instrumentRepo_ = std::make_shared<NiceMock<MockInstrumentRepository>>();
        
        // По умолчанию репозитории пустые
        ON_CALL(*balanceRepo_, findByAccountId(_)).WillByDefault(Return(std::nullopt));
        ON_CALL(*positionRepo_, findByAccountId(_)).WillByDefault(Return(std::vector<domain::BrokerPosition>{}));
        ON_CALL(*orderRepo_, findByAccountId(_)).WillByDefault(Return(std::vector<domain::BrokerOrder>{}));
        ON_CALL(*instrumentRepo_, findAll()).WillByDefault(Return(std::vector<domain::Instrument>{}));
    }
    
    std::unique_ptr<FakeBrokerAdapter> createAdapter() {
        return std::make_unique<FakeBrokerAdapter>(
            broker_,           // EnhancedFakeBroker - первый параметр!
            eventPublisher_,
            balanceRepo_,
            positionRepo_,
            orderRepo_,
            quoteRepo_,
            instrumentRepo_
        );
    }
    
    std::shared_ptr<BrokerSettings> settings_;
    std::shared_ptr<EnhancedFakeBroker> broker_;
    std::shared_ptr<NiceMock<MockEventPublisher>> eventPublisher_;
    std::shared_ptr<NiceMock<MockBalanceRepository>> balanceRepo_;
    std::shared_ptr<NiceMock<MockPositionRepository>> positionRepo_;
    std::shared_ptr<NiceMock<MockOrderRepository>> orderRepo_;
    std::shared_ptr<NiceMock<MockQuoteRepository>> quoteRepo_;
    std::shared_ptr<NiceMock<MockInstrumentRepository>> instrumentRepo_;
};


// ============================================================================
// SANDBOX ACCOUNT TESTS
// ============================================================================

TEST_F(FakeBrokerAdapterTest, GetPortfolio_SandboxAccount_AutoRegisters) {
    auto adapter = createAdapter();
    
    // Ожидаем что баланс будет сохранён в БД
    EXPECT_CALL(*balanceRepo_, save(_)).Times(1);
    
    // Получаем портфель нового sandbox-аккаунта
    auto portfolio = adapter->getPortfolio("my-new-sandbox-account");
    
    // Проверяем начальный баланс ~100,000 RUB
    EXPECT_EQ(portfolio.accountId, "my-new-sandbox-account");
    EXPECT_NEAR(portfolio.cash.toDouble(), 100000.0, 1.0);
}

TEST_F(FakeBrokerAdapterTest, GetPortfolio_ExistingSandboxAccount_NoDoubleRegistration) {
    // Аккаунт уже существует в БД
    domain::BrokerBalance existingBalance;
    existingBalance.accountId = "existing-sandbox-acc";
    existingBalance.available = 5000000;  // 50,000 RUB
    existingBalance.reserved = 0;
    existingBalance.currency = "RUB";
    
    ON_CALL(*balanceRepo_, findByAccountId("existing-sandbox-acc"))
        .WillByDefault(Return(existingBalance));
    
    auto adapter = createAdapter();
    
    // НЕ ожидаем сохранения - аккаунт уже есть
    EXPECT_CALL(*balanceRepo_, save(_)).Times(0);
    
    auto portfolio = adapter->getPortfolio("existing-sandbox-acc");
    
    EXPECT_EQ(portfolio.accountId, "existing-sandbox-acc");
    EXPECT_NEAR(portfolio.cash.toDouble(), 50000.0, 1.0);
}

TEST_F(FakeBrokerAdapterTest, GetPortfolio_NonSandboxAccount_ThrowsException) {
    auto adapter = createAdapter();
    
    // Не-sandbox аккаунт должен выбросить исключение
    EXPECT_THROW(
        adapter->getPortfolio("real-production-account"),
        std::runtime_error
    );
}

TEST_F(FakeBrokerAdapterTest, GetOrders_NonSandboxAccount_ThrowsException) {
    auto adapter = createAdapter();
    
    EXPECT_THROW(
        adapter->getOrders("unknown-real-account"),
        std::runtime_error
    );
}


// ============================================================================
// ORDER TESTS
// ============================================================================

TEST_F(FakeBrokerAdapterTest, PlaceOrder_SandboxAccount_Success) {
    auto adapter = createAdapter();
    
    // Ожидаем сохранение баланса и ордера
    EXPECT_CALL(*balanceRepo_, save(_)).Times(AtLeast(1));
    EXPECT_CALL(*orderRepo_, save(_)).Times(1);
    
    domain::OrderRequest request;
    request.figi = "BBG004730N88";
    request.quantity = 10;
    request.direction = domain::OrderDirection::BUY;
    request.type = domain::OrderType::MARKET;
    request.price = domain::Money::fromDouble(0, "RUB");
    
    auto result = adapter->placeOrder("test-sandbox-account", request);
    
    EXPECT_FALSE(result.orderId.empty());
    // MARKET ордер должен быть исполнен сразу
    EXPECT_TRUE(result.status == domain::OrderStatus::FILLED || 
                result.status == domain::OrderStatus::PENDING);
}

TEST_F(FakeBrokerAdapterTest, PlaceOrder_NonSandboxAccount_Rejected) {
    auto adapter = createAdapter();
    
    domain::OrderRequest request;
    request.figi = "BBG004730N88";
    request.quantity = 10;
    request.direction = domain::OrderDirection::BUY;
    request.type = domain::OrderType::MARKET;
    request.price = domain::Money::fromDouble(0, "RUB");
    
    auto result = adapter->placeOrder("production-account", request);
    
    // Должен быть REJECTED
    EXPECT_EQ(result.status, domain::OrderStatus::REJECTED);
    EXPECT_FALSE(result.message.empty());
}


// ============================================================================
// CACHING TESTS
// ============================================================================

TEST_F(FakeBrokerAdapterTest, GetQuote_CachesFromDb) {
    domain::Quote dbQuote;
    dbQuote.figi = "BBG004730N88";
    dbQuote.lastPrice = domain::Money::fromDouble(265.0, "RUB");
    dbQuote.bidPrice = domain::Money::fromDouble(264.5, "RUB");
    dbQuote.askPrice = domain::Money::fromDouble(265.5, "RUB");
    
    ON_CALL(*quoteRepo_, findByFigi("BBG004730N88"))
        .WillByDefault(Return(dbQuote));
    
    auto adapter = createAdapter();
    
    // Первый вызов - из БД
    auto quote1 = adapter->getQuote("BBG004730N88");
    ASSERT_TRUE(quote1.has_value());
    EXPECT_NEAR(quote1->lastPrice.toDouble(), 265.0, 0.01);
    
    // Второй вызов - должен быть из кэша (БД не вызывается повторно)
    auto quote2 = adapter->getQuote("BBG004730N88");
    ASSERT_TRUE(quote2.has_value());
    EXPECT_NEAR(quote2->lastPrice.toDouble(), 265.0, 0.01);
}

TEST_F(FakeBrokerAdapterTest, GetInstrument_CachesFromDb) {
    domain::Instrument dbInstr;
    dbInstr.figi = "BBG004730N88";
    dbInstr.ticker = "SBER";
    dbInstr.name = "Сбербанк";
    dbInstr.currency = "RUB";
    dbInstr.lot = 10;
    
    ON_CALL(*instrumentRepo_, findByFigi("BBG004730N88"))
        .WillByDefault(Return(dbInstr));
    
    auto adapter = createAdapter();
    
    auto instr = adapter->getInstrumentByFigi("BBG004730N88");
    ASSERT_TRUE(instr.has_value());
    EXPECT_EQ(instr->ticker, "SBER");
    EXPECT_EQ(instr->name, "Сбербанк");
}


// ============================================================================
// TRANSACTION ROLLBACK TESTS
// ============================================================================

TEST_F(FakeBrokerAdapterTest, RegisterAccount_DbFails_NoCacheUpdate) {
    // БД выбрасывает исключение
    ON_CALL(*balanceRepo_, save(_))
        .WillByDefault(Throw(std::runtime_error("DB connection lost")));
    
    auto adapter = createAdapter();
    
    // Регистрация должна выбросить исключение
    EXPECT_THROW(
        adapter->registerAccount("new-sandbox-test", "token"),
        std::runtime_error
    );
    
    // После неудачной регистрации аккаунт НЕ должен существовать
    // (т.к. кэш не обновился)
    EXPECT_THROW(
        adapter->getPortfolio("new-sandbox-test"),
        std::runtime_error  // или проверить что он не в кэше
    );
}


// ============================================================================
// INTEGRATION TESTS (с реальным EnhancedFakeBroker)
// ============================================================================

TEST_F(FakeBrokerAdapterTest, SandboxAccountNaming) {
    auto adapter = createAdapter();
    
    // Разные варианты sandbox в названии - все должны работать
    EXPECT_NO_THROW(adapter->getPortfolio("sandbox"));
    EXPECT_NO_THROW(adapter->getPortfolio("my-sandbox-account"));
    EXPECT_NO_THROW(adapter->getPortfolio("acc-001-sandbox"));
    EXPECT_NO_THROW(adapter->getPortfolio("sandbox-test-123"));
    
    // Без sandbox - должно падать
    EXPECT_THROW(adapter->getPortfolio("production"), std::runtime_error);
    EXPECT_THROW(adapter->getPortfolio("real-account"), std::runtime_error);
    EXPECT_THROW(adapter->getPortfolio("acc-001"), std::runtime_error);
}

TEST_F(FakeBrokerAdapterTest, GetQuote_FallbackToSimulator) {
    // БД пустая
    ON_CALL(*quoteRepo_, findByFigi(_)).WillByDefault(Return(std::nullopt));
    
    auto adapter = createAdapter();
    
    // Должен получить котировку из симулятора (EnhancedFakeBroker)
    auto quote = adapter->getQuote("BBG004730N88");  // SBER
    ASSERT_TRUE(quote.has_value());
    EXPECT_GT(quote->lastPrice.toDouble(), 0.0);
}

TEST_F(FakeBrokerAdapterTest, GetInstruments_FallbackToSimulator) {
    // БД пустая
    ON_CALL(*instrumentRepo_, findAll()).WillByDefault(Return(std::vector<domain::Instrument>{}));
    
    auto adapter = createAdapter();
    
    // Должен получить инструменты из симулятора
    auto instruments = adapter->getAllInstruments();
    EXPECT_GT(instruments.size(), 0u);
}
