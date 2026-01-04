// include/application/MarketDataPublisher.hpp
#pragma once

#include "ports/output/IEventPublisher.hpp"
#include "domain/Quote.hpp"
#include "domain/Portfolio.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>
#include <chrono>

namespace broker::application {

/**
 * @brief Публикатор рыночных данных
 * 
 * Публикует события в broker.events exchange:
 * - quote.updated → изменение котировки инструмента
 * - portfolio.updated → изменение портфеля (позиции, баланс)
 */
class MarketDataPublisher {
public:
    explicit MarketDataPublisher(std::shared_ptr<ports::output::IEventPublisher> eventPublisher)
        : eventPublisher_(std::move(eventPublisher))
    {
        std::cout << "[MarketDataPublisher] Created" << std::endl;
    }

    void publishQuoteUpdate(const domain::Quote& quote) {
        nlohmann::json event;
        event["figi"] = quote.figi;
        event["bid"] = quote.bidPrice.toDouble();
        event["ask"] = quote.askPrice.toDouble();
        event["last_price"] = quote.lastPrice.toDouble();
        event["currency"] = quote.lastPrice.currency;
        event["timestamp"] = getCurrentTimestamp();
        eventPublisher_->publish("quote.updated", event.dump());
    }

    void publishPortfolioUpdate(const std::string& accountId, const domain::Portfolio& portfolio) {
        nlohmann::json event;
        event["account_id"] = accountId;
        event["timestamp"] = getCurrentTimestamp();
        event["cash"] = {{"amount", portfolio.cash.toDouble()}, {"currency", portfolio.cash.currency}};
        
        event["positions"] = nlohmann::json::array();
        for (const auto& pos : portfolio.positions) {
            event["positions"].push_back({
                {"figi", pos.figi},
                {"quantity", pos.quantity},
                {"average_price", pos.averagePrice.toDouble()},
                {"current_price", pos.currentPrice.toDouble()}
            });
        }
        
        event["total_value"] = {{"amount", portfolio.totalValue.toDouble()}, {"currency", portfolio.totalValue.currency}};
        eventPublisher_->publish("portfolio.updated", event.dump());
        std::cout << "[MarketDataPublisher] Published portfolio.updated: " << accountId << std::endl;
    }

private:
    int64_t getCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::shared_ptr<ports::output::IEventPublisher> eventPublisher_;
};

} // namespace broker::application
