#pragma once

#include "ports/output/IEventPublisher.hpp"
#include "ports/output/IEventConsumer.hpp"
#include "settings/RabbitMQSettings.hpp"
#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <iostream>

namespace trading::adapters::secondary {

/**
 * @brief RabbitMQ адаптер со строковым интерфейсом
 * 
 * Реализует IEventPublisher и IEventConsumer.
 * Использует строковый интерфейс (routingKey + message).
 * 
 * Архитектура:
 * - Exchange: topic (trading.events)
 * - Routing keys: order.create, order.cancel, order.created, order.rejected, etc.
 * - Очереди: auto-generated exclusive
 * 
 * @example
 * ```cpp
 * auto settings = std::make_shared<RabbitMQSettings>();
 * auto adapter = std::make_shared<RabbitMQAdapter>(settings);
 * 
 * // Подписка
 * adapter->subscribe({"order.created", "order.rejected"}, [](const std::string& key, const std::string& msg) {
 *     std::cout << key << ": " << msg << std::endl;
 * });
 * 
 * adapter->start();
 * 
 * // Публикация
 * adapter->publish("order.create", R"({"order_id":"ord-001"})");
 * ```
 */
class RabbitMQAdapter : public ports::output::IEventPublisher,
                        public ports::output::IEventConsumer {
public:
    explicit RabbitMQAdapter(std::shared_ptr<settings::RabbitMQSettings> settings)
        : settings_(std::move(settings))
        , running_(false)
        , ioContext_()
        , handler_(ioContext_)
    {
        exchangeName_ = settings_->getExchange();
        std::cout << "[RabbitMQAdapter] Created for " 
                  << settings_->getHost() << ":" << settings_->getPort()
                  << " exchange=" << exchangeName_ << std::endl;
                  start();
    }

    ~RabbitMQAdapter() override {
        stop();
    }

    // =========================================================================
    // IEventPublisher
    // =========================================================================
    
    /**
     * @brief Опубликовать событие
     * @param routingKey Ключ маршрутизации
     * @param message JSON-сообщение
     */
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
    
    /**
     * @brief Подписаться на события
     * @param routingKeys Список ключей маршрутизации
     * @param handler Обработчик событий
     */
    void subscribe(const std::vector<std::string>& routingKeys, 
                   ports::output::EventHandler handler) override {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        
        for (const auto& key : routingKeys) {
            handlers_[key].push_back(handler);
            pendingBindings_.push_back(key);
        }
    }

    /**
     * @brief Запустить прослушивание
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
        
        connection_ = std::make_unique<AMQP::TcpConnection>(&handler_, 
            AMQP::Address(connStr));
        channel_ = std::make_unique<AMQP::TcpChannel>(connection_.get());
        
        // Объявляем exchange
        channel_->declareExchange(exchangeName_, AMQP::topic, AMQP::durable)
            .onSuccess([this]() {
                std::cout << "[RabbitMQAdapter] Exchange declared: " << exchangeName_ << std::endl;
                setupBindings();
            })
            .onError([](const char* msg) {
                std::cerr << "[RabbitMQAdapter] Exchange error: " << msg << std::endl;
            });
    }

    void setupBindings() {
        // Объявляем exclusive queue
        channel_->declareQueue(AMQP::exclusive)
            .onSuccess([this](const std::string& name, uint32_t, uint32_t) {
                queueName_ = name;
                std::cout << "[RabbitMQAdapter] Queue declared: " << queueName_ << std::endl;
                
                // Биндим все routing keys
                std::lock_guard<std::mutex> lock(handlersMutex_);
                for (const auto& key : pendingBindings_) {
                    channel_->bindQueue(exchangeName_, queueName_, key);
                    std::cout << "[RabbitMQAdapter] Bound: " << key << std::endl;
                }
                pendingBindings_.clear();
                
                // Начинаем консьюминг
                startConsuming();
            });
    }

    void startConsuming() {
        channel_->consume(queueName_)
            .onReceived([this](const AMQP::Message& msg, uint64_t tag, bool) {
                std::string routingKey = msg.routingkey();
                std::string body(msg.body(), msg.bodySize());
                
                std::cout << "[RabbitMQAdapter] Received " << routingKey << std::endl;
                
                // Вызываем handlers
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
                }
                
                channel_->ack(tag);
            })
            .onError([](const char* msg) {
                std::cerr << "[RabbitMQAdapter] Consume error: " << msg << std::endl;
            });
    }

    std::shared_ptr<settings::RabbitMQSettings> settings_;
    std::string exchangeName_;
    std::string queueName_;
    
    std::atomic<bool> running_;
    boost::asio::io_context ioContext_;
    AMQP::LibBoostAsioHandler handler_;
    
    std::unique_ptr<AMQP::TcpConnection> connection_;
    std::unique_ptr<AMQP::TcpChannel> channel_;
    
    std::thread workerThread_;
    
    std::mutex handlersMutex_;
    std::unordered_map<std::string, std::vector<ports::output::EventHandler>> handlers_;
    std::vector<std::string> pendingBindings_;
};

} // namespace trading::adapters::secondary
