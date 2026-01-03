#pragma once

#include "ports/output/IEventPublisher.hpp"
#include <vector>
#include <string>
#include <utility>

namespace trading::tests {

/**
 * @brief Mock реализация IEventPublisher для тестов
 */
class MockEventPublisher : public ports::output::IEventPublisher {
public:
    struct PublishedMessage {
        std::string routingKey;
        std::string message;
    };

    // Получение опубликованных сообщений
    const std::vector<PublishedMessage>& getPublishedMessages() const {
        return messages_;
    }

    void clearMessages() {
        messages_.clear();
    }

    int publishCallCount() const { return messages_.size(); }

    // IEventPublisher implementation
    void publish(const std::string& routingKey, const std::string& message) override {
        messages_.push_back({routingKey, message});
    }

private:
    std::vector<PublishedMessage> messages_;
};

} // namespace trading::tests
