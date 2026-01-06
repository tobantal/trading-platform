#include <gtest/gtest.h>
#include "adapters/secondary/broker/PriceSimulator.hpp"
#include <cmath>
#include <thread>
#include <vector>

using namespace broker::adapters::secondary;

class PriceSimulatorTest : public ::testing::Test {
protected:
    // Используем фиксированный seed для детерминированных тестов
    PriceSimulator simulator{42};
    
    const std::string SBER_FIGI = "BBG004730N88";
    const std::string GAZP_FIGI = "BBG004730RP0";
    
    void SetUp() override {
        simulator.clear();
    }
};

// ================================================================
// INITIALIZATION
// ================================================================

TEST_F(PriceSimulatorTest, InitInstrument_Basic) {
    simulator.initInstrument(SBER_FIGI, 280.0, 0.001, 0.002);
    
    EXPECT_TRUE(simulator.hasInstrument(SBER_FIGI));
    EXPECT_DOUBLE_EQ(simulator.getPrice(SBER_FIGI), 280.0);
}

TEST_F(PriceSimulatorTest, InitInstrument_DefaultSpreadAndVolatility) {
    simulator.initInstrument(SBER_FIGI, 280.0);
    
    auto quote = simulator.getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    
    // Спред по умолчанию 0.1%
    double expectedSpread = 280.0 * 0.001;
    EXPECT_NEAR(quote->spreadAbs(), expectedSpread, 0.01);
}

TEST_F(PriceSimulatorTest, InitInstrument_MultipleInstruments) {
    simulator.initInstrument(SBER_FIGI, 280.0);
    simulator.initInstrument(GAZP_FIGI, 160.0);
    
    EXPECT_EQ(simulator.size(), 2u);
    EXPECT_TRUE(simulator.hasInstrument(SBER_FIGI));
    EXPECT_TRUE(simulator.hasInstrument(GAZP_FIGI));
}

TEST_F(PriceSimulatorTest, HasInstrument_NotFound) {
    EXPECT_FALSE(simulator.hasInstrument("UNKNOWN"));
}

// ================================================================
// QUOTES
// ================================================================

TEST_F(PriceSimulatorTest, GetQuote_BidAskSpread) {
    simulator.initInstrument(SBER_FIGI, 280.0, 0.01, 0.002);  // 1% спред
    
    auto quote = simulator.getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    
    // При цене 280 и спреде 1%:
    // bid = 280 * (1 - 0.005) = 278.6
    // ask = 280 * (1 + 0.005) = 281.4
    EXPECT_NEAR(quote->bid, 278.6, 0.1);
    EXPECT_NEAR(quote->ask, 281.4, 0.1);
    EXPECT_DOUBLE_EQ(quote->last, 280.0);
}

TEST_F(PriceSimulatorTest, GetQuote_MidPrice) {
    simulator.initInstrument(SBER_FIGI, 280.0, 0.01, 0.002);
    
    auto quote = simulator.getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    
    EXPECT_NEAR(quote->mid(), 280.0, 0.01);
}

TEST_F(PriceSimulatorTest, GetQuote_SpreadPercent) {
    simulator.initInstrument(SBER_FIGI, 280.0, 0.01, 0.002);  // 1% спред
    
    auto quote = simulator.getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    
    EXPECT_NEAR(quote->spreadPercent(), 1.0, 0.01);
}

TEST_F(PriceSimulatorTest, GetQuote_NotFound) {
    auto quote = simulator.getQuote("UNKNOWN");
    EXPECT_FALSE(quote.has_value());
}

TEST_F(PriceSimulatorTest, GetQuote_HasTimestamp) {
    auto before = std::chrono::system_clock::now();
    simulator.initInstrument(SBER_FIGI, 280.0);
    
    auto quote = simulator.getQuote(SBER_FIGI);
    auto after = std::chrono::system_clock::now();
    
    ASSERT_TRUE(quote.has_value());
    EXPECT_GE(quote->timestamp, before);
    EXPECT_LE(quote->timestamp, after);
}

// ================================================================
// PRICE MANIPULATION
// ================================================================

TEST_F(PriceSimulatorTest, SetPrice_Success) {
    simulator.initInstrument(SBER_FIGI, 280.0);
    
    bool result = simulator.setPrice(SBER_FIGI, 290.0);
    
    EXPECT_TRUE(result);
    EXPECT_DOUBLE_EQ(simulator.getPrice(SBER_FIGI), 290.0);
}

TEST_F(PriceSimulatorTest, SetPrice_MinimumPrice) {
    simulator.initInstrument(SBER_FIGI, 280.0);
    
    simulator.setPrice(SBER_FIGI, -10.0);  // Попытка установить отрицательную цену
    
    EXPECT_DOUBLE_EQ(simulator.getPrice(SBER_FIGI), 0.01);  // Минимум 1 копейка
}

TEST_F(PriceSimulatorTest, SetPrice_NotFound) {
    bool result = simulator.setPrice("UNKNOWN", 100.0);
    EXPECT_FALSE(result);
}

TEST_F(PriceSimulatorTest, MovePrice_Positive) {
    simulator.initInstrument(SBER_FIGI, 280.0);
    
    double newPrice = simulator.movePrice(SBER_FIGI, 5.0);
    
    EXPECT_DOUBLE_EQ(newPrice, 285.0);
    EXPECT_DOUBLE_EQ(simulator.getPrice(SBER_FIGI), 285.0);
}

TEST_F(PriceSimulatorTest, MovePrice_Negative) {
    simulator.initInstrument(SBER_FIGI, 280.0);
    
    double newPrice = simulator.movePrice(SBER_FIGI, -10.0);
    
    EXPECT_DOUBLE_EQ(newPrice, 270.0);
}

TEST_F(PriceSimulatorTest, MovePrice_NotBelowMinimum) {
    simulator.initInstrument(SBER_FIGI, 1.0);
    
    double newPrice = simulator.movePrice(SBER_FIGI, -100.0);
    
    EXPECT_DOUBLE_EQ(newPrice, 0.01);  // Минимум
}

TEST_F(PriceSimulatorTest, MovePrice_NotFound) {
    double result = simulator.movePrice("UNKNOWN", 5.0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST_F(PriceSimulatorTest, MovePricePercent_Positive) {
    simulator.initInstrument(SBER_FIGI, 200.0);
    
    double newPrice = simulator.movePricePercent(SBER_FIGI, 10.0);  // +10%
    
    EXPECT_DOUBLE_EQ(newPrice, 220.0);
}

TEST_F(PriceSimulatorTest, MovePricePercent_Negative) {
    simulator.initInstrument(SBER_FIGI, 200.0);
    
    double newPrice = simulator.movePricePercent(SBER_FIGI, -5.0);  // -5%
    
    EXPECT_DOUBLE_EQ(newPrice, 190.0);
}

// ================================================================
// TICK SIMULATION
// ================================================================

TEST_F(PriceSimulatorTest, Tick_ChangesPrice) {
    simulator.initInstrument(SBER_FIGI, 280.0, 0.001, 0.1);  // Высокая волатильность
    
    double originalPrice = simulator.getPrice(SBER_FIGI);
    
    // После множества тиков цена должна измениться
    for (int i = 0; i < 100; ++i) {
        simulator.tick(SBER_FIGI);
    }
    
    double newPrice = simulator.getPrice(SBER_FIGI);
    EXPECT_NE(newPrice, originalPrice);
}

TEST_F(PriceSimulatorTest, Tick_NotFound) {
    double result = simulator.tick("UNKNOWN");
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST_F(PriceSimulatorTest, Tick_NeverNegative) {
    simulator.initInstrument(SBER_FIGI, 1.0, 0.001, 0.5);  // Очень высокая волатильность
    
    for (int i = 0; i < 1000; ++i) {
        double price = simulator.tick(SBER_FIGI);
        EXPECT_GE(price, 0.01);
    }
}

TEST_F(PriceSimulatorTest, Simulate_MultipleTicksAtOnce) {
    simulator.initInstrument(SBER_FIGI, 280.0, 0.001, 0.01);
    
    double result = simulator.simulate(SBER_FIGI, 50);
    
    EXPECT_GT(result, 0.0);
    EXPECT_EQ(result, simulator.getPrice(SBER_FIGI));
}

TEST_F(PriceSimulatorTest, Tick_StatisticalProperties) {
    // При нулевом drift, среднее изменение должно быть около 0
    PriceSimulator sim(12345);  // Фиксированный seed
    sim.initInstrument(SBER_FIGI, 100.0, 0.0, 0.01);
    
    double sumReturns = 0.0;
    double prevPrice = 100.0;
    int N = 10000;
    
    for (int i = 0; i < N; ++i) {
        double newPrice = sim.tick(SBER_FIGI);
        double logReturn = std::log(newPrice / prevPrice);
        sumReturns += logReturn;
        prevPrice = newPrice;
    }
    
    double meanReturn = sumReturns / N;
    EXPECT_NEAR(meanReturn, 0.0, 0.001);  // Средняя доходность около 0
}

// ================================================================
// VOLATILITY AND SPREAD MODIFICATION
// ================================================================

TEST_F(PriceSimulatorTest, SetVolatility_Success) {
    simulator.initInstrument(SBER_FIGI, 280.0, 0.001, 0.002);
    
    bool result = simulator.setVolatility(SBER_FIGI, 0.05);
    
    EXPECT_TRUE(result);
}

TEST_F(PriceSimulatorTest, SetVolatility_NotFound) {
    bool result = simulator.setVolatility("UNKNOWN", 0.05);
    EXPECT_FALSE(result);
}

TEST_F(PriceSimulatorTest, SetSpread_Success) {
    simulator.initInstrument(SBER_FIGI, 280.0, 0.001, 0.002);
    
    bool result = simulator.setSpread(SBER_FIGI, 0.02);  // 2%
    
    EXPECT_TRUE(result);
    
    auto quote = simulator.getQuote(SBER_FIGI);
    ASSERT_TRUE(quote.has_value());
    EXPECT_NEAR(quote->spreadPercent(), 2.0, 0.01);
}

// ================================================================
// REMOVAL AND CLEAR
// ================================================================

TEST_F(PriceSimulatorTest, RemoveInstrument_Success) {
    simulator.initInstrument(SBER_FIGI, 280.0);
    
    bool result = simulator.removeInstrument(SBER_FIGI);
    
    EXPECT_TRUE(result);
    EXPECT_FALSE(simulator.hasInstrument(SBER_FIGI));
}

TEST_F(PriceSimulatorTest, RemoveInstrument_NotFound) {
    bool result = simulator.removeInstrument("UNKNOWN");
    EXPECT_FALSE(result);
}

TEST_F(PriceSimulatorTest, Clear_RemovesAll) {
    simulator.initInstrument(SBER_FIGI, 280.0);
    simulator.initInstrument(GAZP_FIGI, 160.0);
    EXPECT_EQ(simulator.size(), 2u);
    
    simulator.clear();
    
    EXPECT_EQ(simulator.size(), 0u);
    EXPECT_FALSE(simulator.hasInstrument(SBER_FIGI));
    EXPECT_FALSE(simulator.hasInstrument(GAZP_FIGI));
}

// ================================================================
// THREAD SAFETY
// ================================================================

TEST_F(PriceSimulatorTest, ThreadSafety_ConcurrentTicks) {
    simulator.initInstrument(SBER_FIGI, 280.0, 0.001, 0.002);
    
    std::vector<std::thread> threads;
    const int numThreads = 4;
    const int ticksPerThread = 1000;
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, ticksPerThread]() {
            for (int j = 0; j < ticksPerThread; ++j) {
                simulator.tick(SBER_FIGI);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Цена должна быть валидной
    double price = simulator.getPrice(SBER_FIGI);
    EXPECT_GT(price, 0.0);
}

TEST_F(PriceSimulatorTest, ThreadSafety_ConcurrentReadWrite) {
    simulator.initInstrument(SBER_FIGI, 280.0, 0.001, 0.002);
    
    std::atomic<bool> running{true};
    
    // Writer thread
    std::thread writer([this, &running]() {
        while (running) {
            simulator.tick(SBER_FIGI);
        }
    });
    
    // Reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < 3; ++i) {
        readers.emplace_back([this, &running]() {
            while (running) {
                auto quote = simulator.getQuote(SBER_FIGI);
                if (quote) {
                    EXPECT_GT(quote->bid, 0.0);
                    EXPECT_GT(quote->ask, 0.0);
                    EXPECT_LE(quote->bid, quote->ask);
                }
            }
        });
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;
    
    writer.join();
    for (auto& r : readers) {
        r.join();
    }
}

// ================================================================
// DETERMINISTIC SEED
// ================================================================

TEST_F(PriceSimulatorTest, DeterministicSeed_SameResults) {
    PriceSimulator sim1(42);
    PriceSimulator sim2(42);
    
    sim1.initInstrument(SBER_FIGI, 280.0, 0.001, 0.01);
    sim2.initInstrument(SBER_FIGI, 280.0, 0.001, 0.01);
    
    for (int i = 0; i < 100; ++i) {
        double price1 = sim1.tick(SBER_FIGI);
        double price2 = sim2.tick(SBER_FIGI);
        EXPECT_DOUBLE_EQ(price1, price2);
    }
}