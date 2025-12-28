/**
 * @file EventBusTest.cpp
 * @brief Тесты для InMemoryEventBus
 * 
 * Проверяет:
 * - Публикацию и подписку на события
 * - Несколько подписчиков на одно событие
 * - Разные типы событий
 * - Отсутствие вызова при неподписанных событиях
 */

#include <gtest/gtest.h>
#include "adapters/secondary/events/InMemoryEventBus.hpp"
#include "domain/events/OrderCreatedEvent.hpp"
#include "domain/events/OrderFilledEvent.hpp"
#include "domain/events/OrderCancelledEvent.hpp"
#include "domain/events/QuoteUpdatedEvent.hpp"

using namespace trading::adapters::secondary;
using namespace trading::domain;

// ============================================================================
// TEST FIXTURE
// ============================================================================

class EventBusTest : public ::testing::Test {
protected:
    void SetUp() override {
        eventBus_ = std::make_unique<InMemoryEventBus>();
    }

    void TearDown() override {
        eventBus_->clear();
        eventBus_.reset();
    }

    /**
     * @brief Создать OrderCreatedEvent с заданными параметрами
     */
    OrderCreatedEvent createOrderCreatedEvent(
        const std::string& orderId,
        const std::string& accountId,
        const std::string& figi,
        int64_t quantity,
        OrderDirection direction = OrderDirection::BUY,
        OrderType type = OrderType::MARKET
    ) {
        OrderCreatedEvent event;
        event.orderId = orderId;
        event.accountId = accountId;
        event.figi = figi;
        event.quantity = quantity;
        event.direction = direction;
        event.orderType = type;
        return event;
    }

    /**
     * @brief Создать OrderFilledEvent с заданными параметрами
     */
    OrderFilledEvent createOrderFilledEvent(
        const std::string& orderId,
        const std::string& accountId,
        const std::string& figi,
        int64_t quantity,
        const Money& price
    ) {
        OrderFilledEvent event;
        event.orderId = orderId;
        event.accountId = accountId;
        event.figi = figi;
        event.quantity = quantity;
        event.executedPrice = price;
        return event;
    }

    /**
     * @brief Создать QuoteUpdatedEvent с заданными параметрами
     */
    QuoteUpdatedEvent createQuoteUpdatedEvent(
        const std::string& figi,
        const Money& lastPrice,
        const Money& bidPrice,
        const Money& askPrice
    ) {
        QuoteUpdatedEvent event;
        event.figi = figi;
        event.lastPrice = lastPrice;
        event.bidPrice = bidPrice;
        event.askPrice = askPrice;
        return event;
    }

    std::unique_ptr<InMemoryEventBus> eventBus_;
};

// ============================================================================
// BASIC TESTS
// ============================================================================

TEST_F(EventBusTest, Subscribe_AndPublish_CallsHandler) {
    bool handlerCalled = false;
    std::string receivedOrderId;
    
    eventBus_->subscribe("order.created", [&](const DomainEvent& event) {
        handlerCalled = true;
        // Безопасный каст к конкретному типу
        const auto* orderEvent = dynamic_cast<const OrderCreatedEvent*>(&event);
        if (orderEvent) {
            receivedOrderId = orderEvent->orderId;
        }
    });
    
    auto event = createOrderCreatedEvent(
        "order-123",
        "acc-456",
        "BBG004730N88",  // SBER
        10
    );
    
    eventBus_->publish(event);
    
    EXPECT_TRUE(handlerCalled);
    EXPECT_EQ(receivedOrderId, "order-123");
}

TEST_F(EventBusTest, NoSubscriber_NoCall) {
    bool handlerCalled = false;
    
    // Подписываемся на order.filled
    eventBus_->subscribe("order.filled", [&](const DomainEvent& event) {
        handlerCalled = true;
    });
    
    // Публикуем order.created (другой тип!)
    auto event = createOrderCreatedEvent(
        "order-123",
        "acc-456",
        "BBG004730N88",
        10
    );
    
    eventBus_->publish(event);
    
    // Handler не должен быть вызван
    EXPECT_FALSE(handlerCalled);
}

TEST_F(EventBusTest, MultipleSubscribers_AllCalled) {
    int callCount = 0;
    
    // Три подписчика на одно событие
    eventBus_->subscribe("order.created", [&](const DomainEvent& e) { callCount++; });
    eventBus_->subscribe("order.created", [&](const DomainEvent& e) { callCount++; });
    eventBus_->subscribe("order.created", [&](const DomainEvent& e) { callCount++; });
    
    auto event = createOrderCreatedEvent(
        "order-123",
        "acc-456",
        "BBG004730N88",
        10
    );
    
    eventBus_->publish(event);
    
    EXPECT_EQ(callCount, 3);
}

TEST_F(EventBusTest, DifferentEventTypes_CorrectRouting) {
    bool orderCreatedCalled = false;
    bool orderFilledCalled = false;
    bool orderCancelledCalled = false;
    
    eventBus_->subscribe("order.created", [&](const DomainEvent& e) { 
        orderCreatedCalled = true; 
    });
    eventBus_->subscribe("order.filled", [&](const DomainEvent& e) { 
        orderFilledCalled = true; 
    });
    eventBus_->subscribe("order.cancelled", [&](const DomainEvent& e) { 
        orderCancelledCalled = true; 
    });
    
    // Публикуем только OrderFilledEvent
    auto event = createOrderFilledEvent(
        "order-123",
        "acc-456",
        "BBG004730N88",
        10,
        Money::fromDouble(250.5, "RUB")
    );
    
    eventBus_->publish(event);
    
    EXPECT_FALSE(orderCreatedCalled);
    EXPECT_TRUE(orderFilledCalled);
    EXPECT_FALSE(orderCancelledCalled);
}

// ============================================================================
// EVENT DATA TESTS
// ============================================================================

TEST_F(EventBusTest, OrderCreatedEvent_ContainsCorrectData) {
    OrderDirection receivedDirection = OrderDirection::SELL;
    OrderType receivedType = OrderType::LIMIT;
    int64_t receivedQuantity = 0;
    std::string receivedFigi;
    
    eventBus_->subscribe("order.created", [&](const DomainEvent& e) {
        const auto* event = dynamic_cast<const OrderCreatedEvent*>(&e);
        if (event) {
            receivedDirection = event->direction;
            receivedType = event->orderType;
            receivedQuantity = event->quantity;
            receivedFigi = event->figi;
        }
    });
    
    auto event = createOrderCreatedEvent(
        "order-999",
        "acc-123",
        "BBG004731032",  // LKOH
        50,
        OrderDirection::BUY,
        OrderType::MARKET
    );
    
    eventBus_->publish(event);
    
    EXPECT_EQ(receivedDirection, OrderDirection::BUY);
    EXPECT_EQ(receivedType, OrderType::MARKET);
    EXPECT_EQ(receivedQuantity, 50);
    EXPECT_EQ(receivedFigi, "BBG004731032");
}

TEST_F(EventBusTest, OrderFilledEvent_ContainsPrice) {
    Money receivedPrice;
    
    eventBus_->subscribe("order.filled", [&](const DomainEvent& e) {
        const auto* event = dynamic_cast<const OrderFilledEvent*>(&e);
        if (event) {
            receivedPrice = event->executedPrice;
        }
    });
    
    auto event = createOrderFilledEvent(
        "order-123",
        "acc-456",
        "BBG004730N88",
        10,
        Money::fromDouble(255.75, "RUB")
    );
    
    eventBus_->publish(event);
    
    EXPECT_EQ(receivedPrice.currency, "RUB");
    // Проверяем примерное значение
    EXPECT_GE(receivedPrice.units, 255);
    EXPECT_LE(receivedPrice.units, 256);
}

TEST_F(EventBusTest, QuoteUpdatedEvent_ContainsPrices) {
    std::string receivedFigi;
    Money receivedBid, receivedAsk;
    
    eventBus_->subscribe("quote.updated", [&](const DomainEvent& e) {
        const auto* event = dynamic_cast<const QuoteUpdatedEvent*>(&e);
        if (event) {
            receivedFigi = event->figi;
            receivedBid = event->bidPrice;
            receivedAsk = event->askPrice;
        }
    });
    
    auto event = createQuoteUpdatedEvent(
        "BBG004730N88",
        Money::fromDouble(250.0, "RUB"),  // last
        Money::fromDouble(249.8, "RUB"),  // bid
        Money::fromDouble(250.2, "RUB")   // ask
    );
    
    eventBus_->publish(event);
    
    EXPECT_EQ(receivedFigi, "BBG004730N88");
    EXPECT_EQ(receivedBid.currency, "RUB");
    EXPECT_EQ(receivedAsk.currency, "RUB");
}

// ============================================================================
// HELPER METHODS TESTS
// ============================================================================

TEST_F(EventBusTest, HasSubscribers_ReturnsTrueWhenExists) {
    EXPECT_FALSE(eventBus_->hasSubscribers("order.created"));
    
    eventBus_->subscribe("order.created", [](const DomainEvent& e) {});
    
    EXPECT_TRUE(eventBus_->hasSubscribers("order.created"));
    EXPECT_FALSE(eventBus_->hasSubscribers("order.filled"));
}

TEST_F(EventBusTest, SubscriberCount_ReturnsCorrectNumber) {
    EXPECT_EQ(eventBus_->subscriberCount("order.created"), 0u);
    
    eventBus_->subscribe("order.created", [](const DomainEvent& e) {});
    EXPECT_EQ(eventBus_->subscriberCount("order.created"), 1u);
    
    eventBus_->subscribe("order.created", [](const DomainEvent& e) {});
    EXPECT_EQ(eventBus_->subscriberCount("order.created"), 2u);
    
    eventBus_->subscribe("order.filled", [](const DomainEvent& e) {});
    EXPECT_EQ(eventBus_->subscriberCount("order.created"), 2u);
    EXPECT_EQ(eventBus_->subscriberCount("order.filled"), 1u);
}

TEST_F(EventBusTest, Unsubscribe_RemovesAllHandlers) {
    int callCount = 0;
    
    eventBus_->subscribe("order.created", [&](const DomainEvent& e) { callCount++; });
    eventBus_->subscribe("order.created", [&](const DomainEvent& e) { callCount++; });
    
    EXPECT_EQ(eventBus_->subscriberCount("order.created"), 2u);
    
    eventBus_->unsubscribe("order.created");
    
    EXPECT_EQ(eventBus_->subscriberCount("order.created"), 0u);
    EXPECT_FALSE(eventBus_->hasSubscribers("order.created"));
    
    // Публикация не должна вызвать handlers
    auto event = createOrderCreatedEvent("order-1", "acc-1", "FIGI", 10);
    eventBus_->publish(event);
    
    EXPECT_EQ(callCount, 0);
}

TEST_F(EventBusTest, Clear_RemovesAllSubscriptions) {
    eventBus_->subscribe("order.created", [](const DomainEvent& e) {});
    eventBus_->subscribe("order.filled", [](const DomainEvent& e) {});
    eventBus_->subscribe("quote.updated", [](const DomainEvent& e) {});
    
    EXPECT_TRUE(eventBus_->hasSubscribers("order.created"));
    EXPECT_TRUE(eventBus_->hasSubscribers("order.filled"));
    EXPECT_TRUE(eventBus_->hasSubscribers("quote.updated"));
    
    eventBus_->clear();
    
    EXPECT_FALSE(eventBus_->hasSubscribers("order.created"));
    EXPECT_FALSE(eventBus_->hasSubscribers("order.filled"));
    EXPECT_FALSE(eventBus_->hasSubscribers("quote.updated"));
}

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(EventBusTest, StartStop_WorksCorrectly) {
    EXPECT_FALSE(eventBus_->isRunning());
    
    eventBus_->start();
    EXPECT_TRUE(eventBus_->isRunning());
    
    eventBus_->stop();
    EXPECT_FALSE(eventBus_->isRunning());
}

// ============================================================================
// EDGE CASES
// ============================================================================

TEST_F(EventBusTest, PublishWithoutSubscribers_NoException) {
    // Не должно быть исключений при публикации без подписчиков
    EXPECT_NO_THROW({
        auto event = createOrderCreatedEvent("order-123", "acc-456", "BBG004730N88", 10);
        eventBus_->publish(event);
    });
}

TEST_F(EventBusTest, MultiplePublish_AllHandled) {
    int callCount = 0;
    
    eventBus_->subscribe("order.created", [&](const DomainEvent& e) { callCount++; });
    
    // Публикуем 5 событий
    for (int i = 0; i < 5; i++) {
        auto event = createOrderCreatedEvent(
            "order-" + std::to_string(i),
            "acc-456",
            "BBG004730N88",
            10
        );
        eventBus_->publish(event);
    }
    
    EXPECT_EQ(callCount, 5);
}

TEST_F(EventBusTest, EventType_MatchesExpected) {
    // Проверяем, что eventType корректно устанавливается
    OrderCreatedEvent created;
    EXPECT_EQ(created.eventType, "order.created");
    
    OrderFilledEvent filled;
    EXPECT_EQ(filled.eventType, "order.filled");
    
    QuoteUpdatedEvent quote;
    EXPECT_EQ(quote.eventType, "quote.updated");
}
