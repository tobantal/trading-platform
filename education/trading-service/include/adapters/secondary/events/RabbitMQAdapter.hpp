// include/adapters/secondary/events/RabbitMQAdapter.hpp
#pragma once

#include "ports/output/IEventPublisher.hpp"
#include "ports/input/IEventConsumer.hpp"
#include "settings/RabbitMQSettings.hpp"
#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>

namespace trading::adapters::secondary {

/**
 * @brief RabbitMQ адаптер со строковым интерфейсом
 * 
 * Реализует IEventPublisher и IEventConsumer.
 * 
 * ВАЖНО: start() НЕ вызывается автоматически в конструкторе!
 * Вызывающий код должен:
 * 1. Создать adapter
 * 2. Вызвать subscribe() для регистрации handlers
 * 3. Вызвать start() для запуска подключения
 */
class RabbitMQAdapter : public ports::output::IEventPublisher,
                        public ports::input::IEventConsumer {
public:
    explicit RabbitMQAdapter(std::shared_ptr<settings::RabbitMQSettings> settings)
        : settings_(std::move(settings))
        , running_(false)
        , connected_(false)
        , ioContext_()
        , handler_(ioContext_)
    {
        exchangeName_ = settings_->getExchange();
        std::cout << "[RabbitMQAdapter] Created for " 
                  << settings_->getHost() << ":" << settings_->getPort()
                  << " exchange=" << exchangeName_ << std::endl;
        // НЕ вызываем start() здесь!
    }

    ~RabbitMQAdapter() override {
        stop();
    }

    // =========================================================================
    // IEventPublisher
    // =========================================================================
    
    void publish(const std::string& routingKey, const std::string& message) override {
        if (!channel_ || !running_) {
            std::cerr << "[RabbitMQAdapter] Cannot publish: not connected" << std::endl;
            return;
        }

        try {
            channel_->publish(exchangeName_, routingKey, message);
            std::cout << "[RabbitMQAdapter] Published " << routingKey 
                      << ": " << message.substr(0, 100) << "..." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[RabbitMQAdapter] Publish error: " << e.what() << std::endl;
        }
    }

    // =========================================================================
    // IEventConsumer
    // =========================================================================
    
    void subscribe(const std::vector<std::string>& routingKeys, 
                   ports::input::EventHandler handler) override {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        
        for (const auto& key : routingKeys) {
            handlers_[key].push_back(handler);
            pendingBindings_.push_back(key);
            std::cout << "[RabbitMQAdapter] Registered handler for: " << key << std::endl;
        }
        
        // Если уже подключены - сразу делаем binding
        if (connected_ && channel_) {
            applyPendingBindings();
        }
    }

    /**
     * @brief Запустить прослушивание
     * 
     * ВАЖНО: Вызывайте ПОСЛЕ того как все subscribe() сделаны!
     */
    void start() override {
        if (running_) return;
        
        running_ = true;
        
        workerThread_ = std::thread([this]() {
            try {
                connect();
                ioContext_.run();
            } catch (const std::exception& e) {
                std::cerr << "[RabbitMQAdapter] Worker error: " << e.what() << std::endl;
            }
        });
        
        std::cout << "[RabbitMQAdapter] Started" << std::endl;
    }

    /**
     * @brief Остановить прослушивание
     */
    void stop() override {
        if (!running_) return;
        
        running_ = false;
        connected_ = false;
        ioContext_.stop();
        
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
        
        channel_.reset();
        connection_.reset();
        
        std::cout << "[RabbitMQAdapter] Stopped" << std::endl;
    }

private:
    void connect() {
        std::string connStr = "amqp://" + settings_->getUser() + ":" + 
                              settings_->getPassword() + "@" +
                              settings_->getHost() + ":" + 
                              std::to_string(settings_->getPort()) + "/";
        
        std::cout << "[RabbitMQAdapter] Connecting to " << settings_->getHost() 
                  << ":" << settings_->getPort() << std::endl;
        
        connection_ = std::make_unique<AMQP::TcpConnection>(&handler_, 
            AMQP::Address(connStr));
        channel_ = std::make_unique<AMQP::TcpChannel>(connection_.get());
        
        // Объявляем exchange
        channel_->declareExchange(exchangeName_, AMQP::topic, AMQP::durable)
            .onSuccess([this]() {
                std::cout << "[RabbitMQAdapter] Exchange declared: " << exchangeName_ << std::endl;
                setupQueue();
            })
            .onError([](const char* msg) {
                std::cerr << "[RabbitMQAdapter] Exchange error: " << msg << std::endl;
            });
    }

    void setupQueue() {
        // Объявляем exclusive queue
        channel_->declareQueue(AMQP::exclusive)
            .onSuccess([this](const std::string& name, uint32_t, uint32_t) {
                queueName_ = name;
                std::cout << "[RabbitMQAdapter] Queue declared: " << queueName_ << std::endl;
                
                connected_ = true;
                
                // Применяем все накопленные bindings
                applyPendingBindings();
                
                // Начинаем консьюминг
                startConsuming();
            })
            .onError([](const char* msg) {
                std::cerr << "[RabbitMQAdapter] Queue error: " << msg << std::endl;
            });
    }

    void applyPendingBindings() {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        
        if (pendingBindings_.empty()) {
            std::cout << "[RabbitMQAdapter] No pending bindings to apply" << std::endl;
            return;
        }
        
        std::cout << "[RabbitMQAdapter] Applying " << pendingBindings_.size() << " bindings..." << std::endl;
        
        for (const auto& key : pendingBindings_) {
            channel_->bindQueue(exchangeName_, queueName_, key)
                .onSuccess([key]() {
                    std::cout << "[RabbitMQAdapter] Bound: " << key << std::endl;
                })
                .onError([key](const char* msg) {
                    std::cerr << "[RabbitMQAdapter] Bind error for " << key << ": " << msg << std::endl;
                });
        }
        pendingBindings_.clear();
    }

    void startConsuming() {
        std::cout << "[RabbitMQAdapter] Starting consumer on queue: " << queueName_ << std::endl;
        
        channel_->consume(queueName_)
            .onReceived([this](const AMQP::Message& msg, uint64_t tag, bool) {
                std::string routingKey = msg.routingkey();
                std::string body(msg.body(), msg.bodySize());
                
                std::cout << "[RabbitMQAdapter] Received " << routingKey 
                          << " (" << body.size() << " bytes)" << std::endl;
                
                // Вызов обработчиков
                std::lock_guard<std::mutex> lock(handlersMutex_);
                auto it = handlers_.find(routingKey);
                if (it != handlers_.end()) {
                    for (const auto& handler : it->second) {
                        try {
                            handler(routingKey, body);
                        } catch (const std::exception& e) {
                            std::cerr << "[RabbitMQAdapter] Handler error: " << e.what() << std::endl;
                        }
                    }
                } else {
                    std::cout << "[RabbitMQAdapter] No handler for: " << routingKey << std::endl;
                }
                
                // ACK только ПОСЛЕ успешной обработки
                channel_->ack(tag);
            })
            .onError([](const char* msg) {
                std::cerr << "[RabbitMQAdapter] Consume error: " << msg << std::endl;
            });
        
        std::cout << "[RabbitMQAdapter] Consumer started, waiting for messages..." << std::endl;
    }

    std::shared_ptr<settings::RabbitMQSettings> settings_;
    std::string exchangeName_;
    std::string queueName_;
    
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    boost::asio::io_context ioContext_;
    AMQP::LibBoostAsioHandler handler_;
    
    std::unique_ptr<AMQP::TcpConnection> connection_;
    std::unique_ptr<AMQP::TcpChannel> channel_;
    
    std::thread workerThread_;
    
    std::mutex handlersMutex_;
    std::unordered_map<std::string, std::vector<ports::input::EventHandler>> handlers_;
    std::vector<std::string> pendingBindings_;
};

} // namespace trading::adapters::secondary
