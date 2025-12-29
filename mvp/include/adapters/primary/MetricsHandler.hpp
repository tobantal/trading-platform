// include/adapters/primary/MetricsHandler.hpp
#pragma once

#include <IHttpHandler.hpp>
#include "ports/output/IEventBus.hpp"
#include "domain/events/OrderCreatedEvent.hpp"
#include "domain/events/OrderFilledEvent.hpp"
#include "domain/events/OrderCancelledEvent.hpp"
#include "domain/events/StrategySignalEvent.hpp"
#include "domain/events/QuoteUpdatedEvent.hpp"
#include <atomic>
#include <sstream>
#include <iostream>
#include <chrono>
#include <memory>

namespace trading::adapters::primary {

/**
 * @brief HTTP Handler для Prometheus метрик с подпиской на события
 * 
 * Endpoint: GET /metrics
 * 
 * Автоматически подписывается на события через IEventBus в конструкторе.
 * Все зависимости через DI.
 */
class MetricsHandler : public IHttpHandler {
public:
    /**
     * @brief Конструктор с DI
     * 
     * Автоматически подписывается на все события.
     * Boost.DI создаёт объект и передаёт IEventBus.
     */
    explicit MetricsHandler(std::shared_ptr<ports::output::IEventBus> eventBus)
        : eventBus_(std::move(eventBus))
        , startTime_(std::chrono::steady_clock::now())
    {
        std::cout << "[MetricsHandler] Created with EventBus" << std::endl;
        subscribeToEvents();
    }

    void handle(IRequest& req, IResponse& res) override {
        ++httpRequestsTotal_;
        
        std::string metrics = buildPrometheusMetrics();
        
        res.setStatus(200);
        res.setHeader("Content-Type", "text/plain; charset=utf-8");
        res.setBody(metrics);
    }

private:
    /**
     * @brief Подписка на все доменные события
     * 
     * Вызывается автоматически в конструкторе.
     */
    void subscribeToEvents() {
        if (!eventBus_) {
            std::cerr << "[MetricsHandler] No EventBus, skipping subscriptions" << std::endl;
            return;
        }

        // Order events
        eventBus_->subscribe("order.created", [this](const domain::DomainEvent& e) {
            ++ordersCreated_;
            ++ordersTotal_;
            
            if (const auto* event = dynamic_cast<const domain::OrderCreatedEvent*>(&e)) {
                if (event->direction == domain::OrderDirection::BUY) {
                    ++ordersBuy_;
                } else {
                    ++ordersSell_;
                }
            }
        });

        eventBus_->subscribe("order.filled", [this](const domain::DomainEvent& e) {
            ++ordersFilled_;
            
            if (const auto* event = dynamic_cast<const domain::OrderFilledEvent*>(&e)) {
                executedVolume_ += event->quantity;
            }
        });

        eventBus_->subscribe("order.cancelled", [this](const domain::DomainEvent&) {
            ++ordersCancelled_;
        });

        // Strategy events
        eventBus_->subscribe("strategy.signal", [this](const domain::DomainEvent& e) {
            ++strategySignalsTotal_;
            
            if (const auto* event = dynamic_cast<const domain::StrategySignalEvent*>(&e)) {
                if (event->signal == domain::SignalType::BUY) {
                    ++signalsBuy_;
                } else if (event->signal == domain::SignalType::SELL) {
                    ++signalsSell_;
                }
            }
        });

        eventBus_->subscribe("strategy.started", [this](const domain::DomainEvent&) {
            ++strategiesActive_;
        });

        eventBus_->subscribe("strategy.stopped", [this](const domain::DomainEvent&) {
            if (strategiesActive_ > 0) --strategiesActive_;
        });

        // Quote events
        eventBus_->subscribe("quote.updated", [this](const domain::DomainEvent&) {
            ++quotesUpdated_;
        });

        std::cout << "[MetricsHandler] Subscribed to 7 event types" << std::endl;
    }

    std::string buildPrometheusMetrics() const {
        std::ostringstream oss;
        
        // Uptime
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - startTime_).count();
        
        oss << "# HELP trading_uptime_seconds Time since application start\n";
        oss << "# TYPE trading_uptime_seconds gauge\n";
        oss << "trading_uptime_seconds " << uptime << "\n\n";

        // HTTP requests
        oss << "# HELP trading_http_requests_total Total HTTP requests\n";
        oss << "# TYPE trading_http_requests_total counter\n";
        oss << "trading_http_requests_total " << httpRequestsTotal_.load() << "\n\n";

        // Orders by status
        oss << "# HELP trading_orders_total Total orders by status\n";
        oss << "# TYPE trading_orders_total counter\n";
        oss << "trading_orders_total{status=\"created\"} " << ordersCreated_.load() << "\n";
        oss << "trading_orders_total{status=\"filled\"} " << ordersFilled_.load() << "\n";
        oss << "trading_orders_total{status=\"cancelled\"} " << ordersCancelled_.load() << "\n\n";

        // Orders by direction
        oss << "# HELP trading_orders_by_direction Orders by direction\n";
        oss << "# TYPE trading_orders_by_direction counter\n";
        oss << "trading_orders_by_direction{direction=\"buy\"} " << ordersBuy_.load() << "\n";
        oss << "trading_orders_by_direction{direction=\"sell\"} " << ordersSell_.load() << "\n\n";

        // Strategy signals
        oss << "# HELP trading_strategy_signals_total Strategy signals by type\n";
        oss << "# TYPE trading_strategy_signals_total counter\n";
        oss << "trading_strategy_signals_total{type=\"buy\"} " << signalsBuy_.load() << "\n";
        oss << "trading_strategy_signals_total{type=\"sell\"} " << signalsSell_.load() << "\n\n";

        // Active strategies
        oss << "# HELP trading_strategies_active Number of active strategies\n";
        oss << "# TYPE trading_strategies_active gauge\n";
        oss << "trading_strategies_active " << strategiesActive_.load() << "\n\n";

        // Executed volume
        oss << "# HELP trading_executed_volume_total Total executed volume in lots\n";
        oss << "# TYPE trading_executed_volume_total counter\n";
        oss << "trading_executed_volume_total " << executedVolume_.load() << "\n\n";

        // Quotes updated
        oss << "# HELP trading_quotes_updated_total Total quote updates\n";
        oss << "# TYPE trading_quotes_updated_total counter\n";
        oss << "trading_quotes_updated_total " << quotesUpdated_.load() << "\n\n";

        // Order fill rate
        int64_t total = ordersTotal_.load();
        int64_t filled = ordersFilled_.load();
        double fillRate = (total > 0) ? static_cast<double>(filled) / total : 0.0;
        
        oss << "# HELP trading_order_fill_rate Order fill rate\n";
        oss << "# TYPE trading_order_fill_rate gauge\n";
        oss << "trading_order_fill_rate " << fillRate << "\n";

        return oss.str();
    }

    std::shared_ptr<ports::output::IEventBus> eventBus_;
    std::chrono::steady_clock::time_point startTime_;

    // Counters
    std::atomic<int64_t> httpRequestsTotal_{0};
    std::atomic<int64_t> ordersTotal_{0};
    std::atomic<int64_t> ordersCreated_{0};
    std::atomic<int64_t> ordersFilled_{0};
    std::atomic<int64_t> ordersCancelled_{0};
    std::atomic<int64_t> ordersBuy_{0};
    std::atomic<int64_t> ordersSell_{0};
    std::atomic<int64_t> strategySignalsTotal_{0};
    std::atomic<int64_t> signalsBuy_{0};
    std::atomic<int64_t> signalsSell_{0};
    std::atomic<int64_t> strategiesActive_{0};
    std::atomic<int64_t> executedVolume_{0};
    std::atomic<int64_t> quotesUpdated_{0};
};

} // namespace trading::adapters::primary