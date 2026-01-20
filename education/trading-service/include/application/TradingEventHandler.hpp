// trading-service/include/application/TradingEventHandler.hpp
#pragma once

#include "ports/input/IEventConsumer.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>
#include <functional>
#include <map>
#include <mutex>
#include <optional>

namespace trading::application {

/**
 * @brief Обработчик событий от broker-service
 * 
 * Слушает события из broker.events exchange:
 * - order.created, order.filled, order.partially_filled, order.rejected, order.cancelled
 * - quote.updated
 * - portfolio.updated
 */
class TradingEventHandler {
public:
    struct OrderUpdate {
        std::string orderId;
        std::string accountId;
        std::string figi;
        std::string status;
        int64_t executedLots = 0;
        double executedPrice = 0.0;
        std::string reason;
        int64_t timestamp = 0;
    };

    struct QuoteUpdate {
        std::string figi;
        double bid = 0.0;
        double ask = 0.0;
        double lastPrice = 0.0;
        std::string currency;
        int64_t timestamp = 0;
    };

    using OrderUpdateCallback = std::function<void(const OrderUpdate&)>;
    using QuoteUpdateCallback = std::function<void(const QuoteUpdate&)>;
    using PortfolioUpdateCallback = std::function<void(const std::string&, const nlohmann::json&)>;

    explicit TradingEventHandler(std::shared_ptr<ports::input::IEventConsumer> eventConsumer)
        : eventConsumer_(std::move(eventConsumer))
    {
        std::cout << "[TradingEventHandler] Created" << std::endl;
        subscribe();
    }

    void onOrderUpdate(OrderUpdateCallback cb) { orderCallback_ = std::move(cb); }
    void onQuoteUpdate(QuoteUpdateCallback cb) { quoteCallback_ = std::move(cb); }
    void onPortfolioUpdate(PortfolioUpdateCallback cb) { portfolioCallback_ = std::move(cb); }

    std::optional<OrderUpdate> getOrderStatus(const std::string& orderId) const {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = orderCache_.find(orderId);
        return (it != orderCache_.end()) ? std::optional(it->second) : std::nullopt;
    }

private:
    void subscribe() {
        eventConsumer_->subscribe(
            {"order.created", "order.filled", "order.partially_filled", 
             "order.rejected", "order.cancelled", "quote.updated", "portfolio.updated"},
            [this](const std::string& key, const std::string& msg) { handleEvent(key, msg); }
        );
        std::cout << "[TradingEventHandler] Subscribed to 7 event types" << std::endl;
    }

    void handleEvent(const std::string& routingKey, const std::string& message) {
        try {
            auto json = nlohmann::json::parse(message);
            
            if (routingKey.rfind("order.", 0) == 0) {
                handleOrderEvent(routingKey, json);
            } else if (routingKey == "quote.updated") {
                handleQuoteEvent(json);
            } else if (routingKey == "portfolio.updated") {
                handlePortfolioEvent(json);
            }
        } catch (const std::exception& e) {
            std::cerr << "[TradingEventHandler] Error: " << e.what() << std::endl;
        }
    }

    // Безопасный парсинг timestamp (может быть числом или строкой)
    static int64_t parseTimestamp(const nlohmann::json& json, const std::string& key) {
        if (!json.contains(key)) return 0;
        
        const auto& val = json[key];
        if (val.is_number()) {
            return val.get<int64_t>();
        } else if (val.is_string()) {
            // Пытаемся распарсить строку как число, или игнорируем
            try {
                return std::stoll(val.get<std::string>());
            } catch (...) {
                return 0;  // ISO timestamp - игнорируем
            }
        }
        return 0;
    }

    void handleOrderEvent(const std::string& routingKey, const nlohmann::json& json) {
        OrderUpdate update;
        update.orderId = json.value("order_id", "");
        update.accountId = json.value("account_id", "");
        update.figi = json.value("figi", "");
        update.status = json.value("status", "");
        update.executedLots = json.value("executed_lots", json.value("filled_lots", 0));
        update.executedPrice = json.value("executed_price", 0.0);
        update.reason = json.value("reason", "");
        update.timestamp = parseTimestamp(json, "timestamp");
        
        std::cout << "[TradingEventHandler] " << routingKey << ": " << update.orderId << std::endl;
        
        {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            orderCache_[update.orderId] = update;
        }
        
        if (orderCallback_) orderCallback_(update);
    }

    void handleQuoteEvent(const nlohmann::json& json) {
        QuoteUpdate update;
        update.figi = json.value("figi", "");
        update.bid = json.value("bid", 0.0);
        update.ask = json.value("ask", 0.0);
        update.lastPrice = json.value("last_price", 0.0);
        update.currency = json.value("currency", "RUB");
        update.timestamp = parseTimestamp(json, "timestamp");
        
        {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            quoteCache_[update.figi] = update;
        }
        
        if (quoteCallback_) quoteCallback_(update);
    }

    void handlePortfolioEvent(const nlohmann::json& json) {
        std::string accountId = json.value("account_id", "");
        std::cout << "[TradingEventHandler] portfolio.updated: " << accountId << std::endl;
        if (portfolioCallback_) portfolioCallback_(accountId, json);
    }

    std::shared_ptr<ports::input::IEventConsumer> eventConsumer_;
    OrderUpdateCallback orderCallback_;
    QuoteUpdateCallback quoteCallback_;
    PortfolioUpdateCallback portfolioCallback_;
    
    mutable std::mutex cacheMutex_;
    std::map<std::string, OrderUpdate> orderCache_;
    std::map<std::string, QuoteUpdate> quoteCache_;
};

} // namespace trading::application
