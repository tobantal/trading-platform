#pragma once

#include "ports/output/IEventBus.hpp"
#include <ThreadSafeMap.hpp>
#include <ThreadSafeQueue.hpp>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

namespace trading::adapters::secondary {

/**
 * @brief In-memory реализация событийной шины
 * 
 * Использует ThreadSafeMap для хранения подписчиков и
 * ThreadSafeQueue для асинхронной обработки событий.
 * 
 * Для MVP - синхронная доставка событий.
 * В Education заменяется на RabbitMqEventBus.
 */
class InMemoryEventBus : public ports::output::IEventBus {
public:
    InMemoryEventBus() : running_(false) {}
    
    ~InMemoryEventBus() override {
        stop();
    }

    /**
     * @brief Опубликовать событие
     * 
     * Синхронно вызывает все зарегистрированные handlers для данного eventType.
     */
    void publish(const domain::DomainEvent& event) override {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        
        auto it = handlers_.find(event.eventType);
        if (it != handlers_.end()) {
            for (const auto& handler : it->second) {
                try {
                    handler(event);
                } catch (const std::exception& e) {
                    // Логируем ошибку, но не прерываем обработку других handlers
                    // В production здесь был бы настоящий логгер
                }
            }
        }
    }

    /**
     * @brief Подписаться на тип события
     */
    void subscribe(const std::string& eventType, ports::output::EventHandler handler) override {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        handlers_[eventType].push_back(std::move(handler));
    }

    /**
     * @brief Отписаться от типа события (удаляет ВСЕ handlers)
     */
    void unsubscribe(const std::string& eventType) override {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        handlers_.erase(eventType);
    }

    /**
     * @brief Проверить наличие подписчиков
     */
    bool hasSubscribers(const std::string& eventType) const override {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        auto it = handlers_.find(eventType);
        return it != handlers_.end() && !it->second.empty();
    }

    /**
     * @brief Запустить обработку (для совместимости с интерфейсом)
     */
    void start() override {
        running_ = true;
    }

    /**
     * @brief Остановить обработку
     */
    void stop() override {
        running_ = false;
    }

    /**
     * @brief Проверить, запущена ли шина
     */
    bool isRunning() const {
        return running_;
    }

    /**
     * @brief Получить количество подписчиков для типа события
     */
    size_t subscriberCount(const std::string& eventType) const {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        auto it = handlers_.find(eventType);
        return it != handlers_.end() ? it->second.size() : 0;
    }

    /**
     * @brief Очистить все подписки (для тестов)
     */
    void clear() {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        handlers_.clear();
    }

private:
    mutable std::mutex handlersMutex_;
    std::unordered_map<std::string, std::vector<ports::output::EventHandler>> handlers_;
    std::atomic<bool> running_;
};

} // namespace trading::adapters::secondary
