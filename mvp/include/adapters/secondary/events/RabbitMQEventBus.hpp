// include/adapters/secondary/events/RabbitMQEventBus.hpp
#pragma once

#include "ports/output/IEventBus.hpp"
#include "ports/output/IRabbitMQSettings.hpp"
#include "domain/events/DomainEvent.hpp"
#include "domain/events/DomainEventFactory.hpp"
#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>
#include <boost/asio.hpp>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <iostream>
#include <memory>

namespace trading::adapters::secondary {

/**
 * @brief RabbitMQ реализация IEventBus
 * 
 * Использует AMQP-CPP с Boost.Asio для асинхронной работы.
 * Настройки подключения получает через IRabbitMQSettings (из IEnvironment).
 * Десериализация событий через DomainEventFactory.
 * 
 * Архитектура:
 * - Exchange: настраивается через settings (default: "trading.events")
 * - Type: topic
 * - Routing keys: eventType (order.created, order.filled, quote.updated, etc.)
 * - Queue per consumer: автогенерируемые exclusive queues
 * 
 * Зависимости:
 * - amqpcpp v4.3.26 (https://github.com/CopernicaMarketingSoftware/AMQP-CPP)
 * - Boost.Asio
 */
class RabbitMQEventBus : public ports::output::IEventBus {
public:
    /**
     * @brief Конструктор с DI
     * @param settings Настройки подключения к RabbitMQ (из IEnvironment)
     * @param eventFactory Фабрика для десериализации событий из JSON
     */
    RabbitMQEventBus(
        std::shared_ptr<ports::output::IRabbitMQSettings> settings,
        std::shared_ptr<domain::DomainEventFactory> eventFactory
    ) : settings_(std::move(settings))
      , eventFactory_(std::move(eventFactory))
      , running_(false)
    {
        exchangeName_ = settings_->getExchange();
        std::cout << "[RabbitMQEventBus] Created for " 
                  << settings_->getHost() << ":" << settings_->getPort() 
                  << " exchange=" << exchangeName_ << std::endl;
                  start();
    }

    ~RabbitMQEventBus() override {
        stop();
    }

    /**
     * @brief Опубликовать событие в RabbitMQ
     */
    void publish(const domain::DomainEvent& event) override {
        if (!running_ || !channel_) {
            std::cerr << "[RabbitMQEventBus] Not connected, cannot publish" << std::endl;
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            std::string routingKey = event.eventType;
            std::string message = event.toJson();
            
            channel_->publish(exchangeName_, routingKey, message);
            
            std::cout << "[RabbitMQEventBus] Published: " << routingKey << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[RabbitMQEventBus] Publish failed: " << e.what() << std::endl;
        }
    }

    /**
     * @brief Подписаться на тип события
     */
    void subscribe(const std::string& eventType, ports::output::EventHandler handler) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        handlers_[eventType].push_back(std::move(handler));
        
        if (running_ && channel_) {
            bindQueueToEvent(eventType);
        }
        
        std::cout << "[RabbitMQEventBus] Subscribed to: " << eventType << std::endl;
    }

    /**
     * @brief Отписаться от типа события
     */
    void unsubscribe(const std::string& eventType) override {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.erase(eventType);
        std::cout << "[RabbitMQEventBus] Unsubscribed from: " << eventType << std::endl;
    }

    /**
     * @brief Проверить наличие подписчиков
     */
    bool hasSubscribers(const std::string& eventType) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handlers_.find(eventType);
        return it != handlers_.end() && !it->second.empty();
    }

    /**
     * @brief Запустить EventBus (подключиться к RabbitMQ)
     */
    void start() override {
        if (running_) return;
        
        std::cout << "[RabbitMQEventBus] Starting..." << std::endl;
        
        running_ = true;
        
        // Запускаем IO в отдельном потоке
        ioThread_ = std::thread([this]() {
            try {
                ioContext_ = std::make_unique<boost::asio::io_context>();
                
                // Создаём AMQP handler для Boost.Asio
                handler_ = std::make_unique<AMQP::LibBoostAsioHandler>(*ioContext_);
                
                // Формируем connection string из settings
                std::string connectionString = "amqp://" 
                    + settings_->getUser() + ":" + settings_->getPassword() 
                    + "@" + settings_->getHost() + ":" + std::to_string(settings_->getPort())
                    + settings_->getVHost();
                
                connection_ = std::make_unique<AMQP::TcpConnection>(
                    handler_.get(), 
                    AMQP::Address(connectionString)
                );
                
                channel_ = std::make_unique<AMQP::TcpChannel>(connection_.get());
                
                // Объявляем exchange
                channel_->declareExchange(exchangeName_, AMQP::topic, AMQP::durable)
                    .onSuccess([this]() {
                        std::cout << "[RabbitMQEventBus] Exchange declared: " << exchangeName_ << std::endl;
                        setupConsumer();
                    })
                    .onError([](const char* msg) {
                        std::cerr << "[RabbitMQEventBus] Exchange error: " << msg << std::endl;
                    });
                
                // Запускаем event loop
                ioContext_->run();
                
            } catch (const std::exception& e) {
                std::cerr << "[RabbitMQEventBus] Connection error: " << e.what() << std::endl;
                running_ = false;
            }
        });
        
        // Даём время на подключение
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::cout << "[RabbitMQEventBus] Started" << std::endl;
    }

    /**
     * @brief Остановить EventBus
     */
    void stop() override {
        if (!running_) return;
        
        std::cout << "[RabbitMQEventBus] Stopping..." << std::endl;
        
        running_ = false;
        
        if (ioContext_) {
            ioContext_->stop();
        }
        
        if (ioThread_.joinable()) {
            ioThread_.join();
        }
        
        channel_.reset();
        connection_.reset();
        handler_.reset();
        ioContext_.reset();
        
        std::cout << "[RabbitMQEventBus] Stopped" << std::endl;
    }

    /**
     * @brief Проверить подключение
     */
    bool isConnected() const {
        return running_ && connection_ && connection_->ready();
    }

private:
    /**
     * @brief Настроить consumer для получения сообщений
     */
    void setupConsumer() {
        if (!channel_) return;
        
        // Создаём exclusive queue для этого consumer
        channel_->declareQueue(AMQP::exclusive)
            .onSuccess([this](const std::string& name, uint32_t, uint32_t) {
                queueName_ = name;
                std::cout << "[RabbitMQEventBus] Queue declared: " << name << std::endl;
                
                // Привязываем ко всем подпискам
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto& [eventType, _] : handlers_) {
                    bindQueueToEvent(eventType);
                }
                
                // Запускаем consume
                channel_->consume(queueName_)
                    .onReceived([this](const AMQP::Message& message, uint64_t deliveryTag, bool) {
                        handleMessage(message, deliveryTag);
                    })
                    .onError([](const char* msg) {
                        std::cerr << "[RabbitMQEventBus] Consume error: " << msg << std::endl;
                    });
            })
            .onError([](const char* msg) {
                std::cerr << "[RabbitMQEventBus] Queue error: " << msg << std::endl;
            });
    }

    /**
     * @brief Привязать очередь к eventType
     */
    void bindQueueToEvent(const std::string& eventType) {
        if (!channel_ || queueName_.empty()) return;
        
        channel_->bindQueue(exchangeName_, queueName_, eventType)
            .onSuccess([eventType]() {
                std::cout << "[RabbitMQEventBus] Bound to: " << eventType << std::endl;
            })
            .onError([eventType](const char* msg) {
                std::cerr << "[RabbitMQEventBus] Bind error for " << eventType << ": " << msg << std::endl;
            });
    }

    /**
     * @brief Обработать входящее сообщение
     * 
     * Использует DomainEventFactory для десериализации JSON в типизированное событие
     */
    void handleMessage(const AMQP::Message& message, uint64_t deliveryTag) {
        std::string routingKey = message.routingkey();
        std::string body(message.body(), message.bodySize());
        
        std::cout << "[RabbitMQEventBus] Received: " << routingKey 
                  << " (" << body.size() << " bytes)" << std::endl;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = handlers_.find(routingKey);
        if (it != handlers_.end()) {
            try {
                // Используем фабрику для создания типизированного события из JSON
                std::unique_ptr<domain::DomainEvent> event = eventFactory_->create(routingKey, body);
                
                // Вызываем все handlers для этого eventType
                for (const auto& handler : it->second) {
                    try {
                        handler(*event);
                    } catch (const std::exception& e) {
                        std::cerr << "[RabbitMQEventBus] Handler error: " << e.what() << std::endl;
                    }
                }
                
            } catch (const std::exception& e) {
                std::cerr << "[RabbitMQEventBus] Event deserialization error: " << e.what() << std::endl;
            }
        }
        
        // ACK
        if (channel_) {
            channel_->ack(deliveryTag);
        }
    }

    // DI зависимости
    std::shared_ptr<ports::output::IRabbitMQSettings> settings_;
    std::shared_ptr<domain::DomainEventFactory> eventFactory_;
    
    // Имя exchange (из settings)
    std::string exchangeName_;
    
    // AMQP-CPP
    std::unique_ptr<boost::asio::io_context> ioContext_;
    std::unique_ptr<AMQP::LibBoostAsioHandler> handler_;
    std::unique_ptr<AMQP::TcpConnection> connection_;
    std::unique_ptr<AMQP::TcpChannel> channel_;
    std::string queueName_;
    
    // Подписки
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<ports::output::EventHandler>> handlers_;
    
    // Lifecycle
    std::atomic<bool> running_;
    std::thread ioThread_;
};

} // namespace trading::adapters::secondary
