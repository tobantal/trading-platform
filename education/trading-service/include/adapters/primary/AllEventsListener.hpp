#pragma once

#include "ports/input/IMetricsService.hpp"
#include "ports/input/IEventConsumer.hpp"

#include <memory>
#include <string>
#include <iostream>

namespace trading::adapters::primary
{

    /**
     * @brief Слушатель всех событий RabbitMQ для сбора метрик
     *
     */
    class AllEventsListener
    {
    public:
        AllEventsListener(
            std::shared_ptr<ports::input::IEventConsumer> consumer,
            std::shared_ptr<ports::input::IMetricsService> metrics) : consumer_(std::move(consumer)), metrics_(std::move(metrics))
        {
            std::cout << "[AllEventsListener] Initializing..." << std::endl;

            consumer_->subscribe(
                {"order.created", "order.filled", "order.rejected",
                 "order.cancelled", "portfolio.updated"},
                [this](const std::string &routingKey, const std::string &message)
                {
                    onEvent(routingKey, message);
                });

            std::cout << "[AllEventsListener] Subscribed to business events" << std::endl;
        }

        void start()
        {
            std::cout << "[AllEventsListener] Starting..." << std::endl;
            consumer_->start();
        }

        void stop()
        {
            std::cout << "[AllEventsListener] Stopping..." << std::endl;
            consumer_->stop();
        }

    private:
        std::shared_ptr<ports::input::IEventConsumer> consumer_; // <-- output, не input
        std::shared_ptr<ports::input::IMetricsService> metrics_;

        void onEvent(const std::string &routingKey, const std::string &message)
        {
            std::cout << "[AllEventsListener] Event: '" << routingKey << "' (len=" << routingKey.length() << ")" << std::endl;

            metrics_->increment("events_received_total", {{"event", routingKey}});

            if (routingKey == "order.created")
            {
                std::cout << "[AllEventsListener] -> orders_created_total++" << std::endl;
                metrics_->increment("orders_created_total");
            }
            else if (routingKey == "order.filled")
            {
                std::cout << "[AllEventsListener] -> orders_filled_total++" << std::endl;
                metrics_->increment("orders_filled_total");
            }
            else if (routingKey == "order.rejected")
            {
                std::cout << "[AllEventsListener] -> orders_rejected_total++" << std::endl;
                metrics_->increment("orders_rejected_total");
            }
            else if (routingKey == "order.cancelled")
            {
                std::cout << "[AllEventsListener] -> orders_cancelled_total++" << std::endl;
                metrics_->increment("orders_cancelled_total");
            }
            else
            {
                std::cout << "[AllEventsListener] -> UNKNOWN EVENT!" << std::endl;
            }
        }
    };

} // namespace trading::adapters::primary
