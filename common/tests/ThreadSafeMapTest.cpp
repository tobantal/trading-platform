#include <gtest/gtest.h>
#include <ThreadSafeMap.hpp>
#include <thread>
#include <vector>
#include <atomic>

struct TestData {
    int value;
    std::string name;
    
    TestData(int v = 0, const std::string& n = "") : value(v), name(n) {}
};

class ThreadSafeMapTest : public ::testing::Test {
protected:
    ThreadSafeMap<std::string, TestData> map;
};

// Базовые тесты
TEST_F(ThreadSafeMapTest, InsertAndFind) {
    auto data = std::make_shared<TestData>(42, "test");
    map.insert("key1", data);
    
    auto found = map.find("key1");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->value, 42);
    EXPECT_EQ(found->name, "test");
}

TEST_F(ThreadSafeMapTest, FindNonExistent) {
    auto found = map.find("nonexistent");
    EXPECT_EQ(found, nullptr);
}

TEST_F(ThreadSafeMapTest, Contains) {
    auto data = std::make_shared<TestData>(10, "exists");
    map.insert("key1", data);
    
    EXPECT_TRUE(map.contains("key1"));
    EXPECT_FALSE(map.contains("key2"));
}

TEST_F(ThreadSafeMapTest, Overwrite) {
    auto data1 = std::make_shared<TestData>(1, "first");
    auto data2 = std::make_shared<TestData>(2, "second");
    
    map.insert("key", data1);
    map.insert("key", data2);
    
    auto found = map.find("key");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->value, 2);
    EXPECT_EQ(found->name, "second");
}

// Корректный многопоточный тест
TEST_F(ThreadSafeMapTest, MultipleWritersSequential) {
    const int NUM_WRITERS = 5;
    const int VALUES_PER_WRITER = 100;
    
    std::vector<std::thread> threads;
    
    // Каждый писатель пишет свои значения в своем диапазоне ключей
    for (int writer = 0; writer < NUM_WRITERS; ++writer) {
        threads.emplace_back([this, writer]() {
            for (int i = 0; i < VALUES_PER_WRITER; ++i) {
                std::string key = "w" + std::to_string(writer) + "_i" + std::to_string(i);
                auto data = std::make_shared<TestData>(writer * 1000 + i, key);
                map.insert(key, data);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // ПРОВЕРЯЕМ все значения - они ДОЛЖНЫ быть там
    for (int writer = 0; writer < NUM_WRITERS; ++writer) {
        for (int i = 0; i < VALUES_PER_WRITER; ++i) {
            std::string key = "w" + std::to_string(writer) + "_i" + std::to_string(i);
            auto found = map.find(key);
            ASSERT_NE(found, nullptr) << "Missing key: " << key;
            EXPECT_EQ(found->value, writer * 1000 + i) << "Wrong value for key: " << key;
        }
    }
}

// Тест на гонку данных при одновременном доступе
TEST_F(ThreadSafeMapTest, ConcurrentReadWrite_NoDataCorruption) {
    const std::string key = "shared_key";
    auto initial_data = std::make_shared<TestData>(0, "initial");
    map.insert(key, initial_data);
    
    std::vector<std::thread> threads;
    std::atomic<int> read_count(0);
    
    // 5 потоков пишут разные значения
    for (int writer = 0; writer < 5; ++writer) {
        threads.emplace_back([this, &key, writer]() {
            for (int i = 0; i < 50; ++i) {
                auto data = std::make_shared<TestData>(writer * 100 + i, "data");
                map.insert(key, data);
            }
        });
    }
    
    // 5 потоков читают - ВСЕГДА должны получить валидный объект
    for (int reader = 0; reader < 5; ++reader) {
        threads.emplace_back([this, &key, &read_count]() {
            for (int i = 0; i < 100; ++i) {
                auto found = map.find(key);
                ASSERT_NE(found, nullptr) << "Reader: found nullptr!";
                read_count++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(read_count, 500);  // 5 readers * 100 reads
}

// Тест гарантии видимости - после insert, find ОБЯЗАТЕЛЬНО вернёт данные
TEST_F(ThreadSafeMapTest, VisibilityGuarantee) {
    std::string key = "visibility_test";
    
    std::thread writer([this, &key]() {
        auto data = std::make_shared<TestData>(42, "visible");
        map.insert(key, data);
    });
    
    writer.join();  // ЖДЁМ завершения писателя
    
    // ТЕПЕРЬ читаем - ДОЛЖНЫ найти
    auto found = map.find(key);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->value, 42);
}
