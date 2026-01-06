#pragma once

#include "MarketScenario.hpp"
#include "PriceSimulator.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>

// TODO: в будущем наверно лучше распилить на 4 процессора для удобства:
// MarketBuy, MarketSell, LimitBuy, LimitSell
namespace broker::adapters::secondary {

/**
 * @brief Направление ордера (локальная копия для изоляции от domain)
 */
enum class Direction { BUY, SELL };

/**
 * @brief Тип ордера
 */
enum class Type { MARKET, LIMIT };

/**
 * @brief Статус ордера
 */
enum class Status {
    PENDING,
    FILLED,
    PARTIALLY_FILLED,
    CANCELLED,
    REJECTED
};

inline std::string toString(Status s) {
    switch (s) {
        case Status::PENDING: return "PENDING";
        case Status::FILLED: return "FILLED";
        case Status::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
        case Status::CANCELLED: return "CANCELLED";
        case Status::REJECTED: return "REJECTED";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Запрос на создание ордера (изолированный от domain)
 */
struct OrderRequest {
    std::string orderId;
    std::string accountId;
    std::string figi;
    Direction direction = Direction::BUY;
    Type type = Type::MARKET;
    int64_t quantity = 0;
    double price = 0.0;  // Для LIMIT ордеров
};

/**
 * @brief Результат обработки ордера
 */
struct OrderResult {
    std::string orderId;
    Status status = Status::PENDING;
    double executedPrice = 0.0;
    int64_t executedQuantity = 0;
    std::string message;
    
    bool isSuccess() const {
        return status == Status::FILLED || status == Status::PARTIALLY_FILLED;
    }
    
    bool isFinal() const {
        return status == Status::FILLED || status == Status::REJECTED || status == Status::CANCELLED;
    }
};

/**
 * @brief Pending ордер в очереди
 */
struct PendingOrder {
    std::string orderId;
    std::string accountId;
    std::string figi;
    Direction direction;
    Type type;
    int64_t quantity;
    double limitPrice;
    std::chrono::system_clock::time_point createdAt; // время, после которого нужно исполнить (для DELAYED)
    std::chrono::system_clock::time_point fillAfter;
    bool isDelayedMarket = false;  // TRUE только для MARKET ордеров!
};

/**
 * @brief Событие исполнения ордера
 */
struct OrderFillEvent {
    std::string orderId;
    std::string accountId;
    std::string figi;
    Direction direction;
    int64_t quantity;
    double price;
    bool partial;
};

/**
 * @brief Callback для событий исполнения
 */
using FillCallback = std::function<void(const OrderFillEvent&)>;

/**
 * @brief Процессор ордеров с реалистичной симуляцией
 * 
 * Обрабатывает ордера согласно MarketScenario:
 * - IMMEDIATE: мгновенное исполнение
 * - REALISTIC: market=fill сразу, limit=pending
 * - PARTIAL: частичное исполнение
 * - DELAYED: отложенное исполнение
 * - ALWAYS_REJECT: всегда отклонять
 */
class OrderProcessor {
public:
    explicit OrderProcessor(std::shared_ptr<PriceSimulator> priceSimulator)
        : priceSimulator_(std::move(priceSimulator))
        , rng_(std::random_device{}())
    {}
    
    void setFillCallback(FillCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        fillCallback_ = std::move(callback);
    }
    
    OrderResult processOrder(const OrderRequest& request, const MarketScenario& scenario) {
        // 1. Проверка rejection
        if (shouldReject(scenario)) {
            return rejectOrder(request, scenario.rejectReason.empty() 
                ? "Order rejected by scenario" 
                : scenario.rejectReason);
        }
        
        // 2. Получаем котировку
        auto quoteOpt = priceSimulator_->getQuote(request.figi);
        if (!quoteOpt) {
            return rejectOrder(request, "Instrument not found: " + request.figi);
        }
        
        const auto& quote = *quoteOpt;
        
        // 3. Обрабатываем согласно fillBehavior
        switch (scenario.fillBehavior) {
            case OrderFillBehavior::IMMEDIATE:
                return fillImmediately(request, quote);
                
            case OrderFillBehavior::REALISTIC:
                return processRealistic(request, quote, scenario);
                
            case OrderFillBehavior::PARTIAL:
                return fillPartially(request, quote, scenario);
                
            case OrderFillBehavior::DELAYED:
                return queueForDelayedFill(request, scenario.fillDelay);
                
            case OrderFillBehavior::ALWAYS_REJECT:
                return rejectOrder(request, scenario.rejectReason.empty() 
                    ? "Always reject mode" 
                    : scenario.rejectReason);
        }
        
        return rejectOrder(request, "Unknown fill behavior");
    }
    
    /**
     * @brief Обработать pending limit-ордера
     */
        void processPendingOrders(const MarketScenario& scenario) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        std::vector<std::string> filledOrders;
        
        for (auto& [orderId, order] : pendingOrders_) {
            auto quoteOpt = priceSimulator_->getQuote(order.figi);
            if (!quoteOpt) continue;
            
            const auto& quote = *quoteOpt;
            bool shouldFill = false;
            double fillPrice = 0.0;
            
            // Разная логика для MARKET и LIMIT
            if (order.isDelayedMarket) {
                // DELAYED MARKET — проверяем только время
                if (now >= order.fillAfter) {
                    shouldFill = true;
                    fillPrice = (order.direction == Direction::BUY) ? quote.ask : quote.bid;
                }
            } else {
                // LIMIT — проверяем цену (стандартная биржевая логика)
                if (order.direction == Direction::BUY) {
                    if (quote.ask <= order.limitPrice) {
                        shouldFill = true;
                        fillPrice = quote.ask;
                    }
                } else {
                    if (quote.bid >= order.limitPrice) {
                        shouldFill = true;
                        fillPrice = quote.bid;
                    }
                }
            }
            
            if (shouldFill) {
                filledOrders.push_back(orderId);
                
                if (fillCallback_) {
                    OrderFillEvent event;
                    event.orderId = orderId;
                    event.accountId = order.accountId;
                    event.figi = order.figi;
                    event.direction = order.direction;
                    event.quantity = order.quantity;
                    event.price = fillPrice;
                    event.partial = false;
                    
                    fillCallback_(event);
                }
            }
        }
        
        for (const auto& orderId : filledOrders) {
            pendingOrders_.erase(orderId);
        }
    }
    
    /**
     * @brief Отменить ордер
     */
    bool cancelOrder(const std::string& orderId) {
        std::lock_guard<std::mutex> lock(mutex_);
        return pendingOrders_.erase(orderId) > 0;
    }
    
    /**
     * @brief Получить pending ордера
     */
    std::vector<PendingOrder> getPendingOrders() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<PendingOrder> result;
        result.reserve(pendingOrders_.size());
        for (const auto& [_, order] : pendingOrders_) {
            result.push_back(order);
        }
        return result;
    }
    
    size_t pendingCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pendingOrders_.size();
    }
    
    /**
     * @brief Очистить все pending ордера
     */
    void clearPending() {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingOrders_.clear();
    }

private:
    std::shared_ptr<PriceSimulator> priceSimulator_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, PendingOrder> pendingOrders_;
    FillCallback fillCallback_;
    std::mt19937 rng_;
    std::atomic<uint64_t> orderCounter_{0};
    
    bool shouldReject(const MarketScenario& scenario) {
        if (scenario.rejectProbability <= 0.0) return false;
        if (scenario.rejectProbability >= 1.0) return true;
        
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng_) < scenario.rejectProbability;
    }
    
    OrderResult rejectOrder(const OrderRequest& request, const std::string& reason) {
        OrderResult result;
        result.orderId = request.orderId;
        result.status = Status::REJECTED;
        result.message = reason;
        return result;
    }
    
    OrderResult fillImmediately(const OrderRequest& request, const PriceSimulator::Quote& quote) {
        double fillPrice = (request.direction == Direction::BUY) ? quote.ask : quote.bid;
        
        OrderResult result;
        result.orderId = request.orderId;
        result.status = Status::FILLED;
        result.executedPrice = fillPrice;
        result.executedQuantity = request.quantity;
        result.message = "Filled immediately";
        
        return result;
    }
    
    OrderResult processRealistic(
        const OrderRequest& request,
        const PriceSimulator::Quote& quote,
        const MarketScenario& scenario)
    {
        if (request.type == Type::MARKET) {
            // Market order: fill сразу с возможным slippage
            double basePrice = (request.direction == Direction::BUY) ? quote.ask : quote.bid;
            
            // Slippage при большом объёме (>10% ликвидности)
            double slippage = 0.0;
            if (request.quantity > scenario.availableLiquidity * 0.1) {
                double ratio = static_cast<double>(request.quantity) / scenario.availableLiquidity;
                slippage = basePrice * scenario.slippagePercent * ratio;
            }
            
            double fillPrice = (request.direction == Direction::BUY)
                ? basePrice + slippage
                : basePrice - slippage;
            
            OrderResult result;
            result.orderId = request.orderId;
            result.status = Status::FILLED;
            result.executedPrice = fillPrice;
            result.executedQuantity = request.quantity;
            result.message = slippage > 0 ? "Filled with slippage" : "Filled";
            
            return result;
        } else {
            // Limit order: проверяем цену и добавляем в pending при необходимости
            bool canFillNow = false;
            double fillPrice = 0.0;
            
            if (request.direction == Direction::BUY) {
                if (quote.ask <= request.price) {
                    canFillNow = true;
                    fillPrice = quote.ask;
                }
            } else {
                if (quote.bid >= request.price) {
                    canFillNow = true;
                    fillPrice = quote.bid;
                }
            }
            
            if (canFillNow) {
                OrderResult result;
                result.orderId = request.orderId;
                result.status = Status::FILLED;
                result.executedPrice = fillPrice;
                result.executedQuantity = request.quantity;
                result.message = "Limit order filled";
                return result;
            } else {
                return queueLimitOrder(request);
            }
        }
    }
    
    OrderResult fillPartially(
        const OrderRequest& request,
        const PriceSimulator::Quote& quote,
        const MarketScenario& scenario)
    {
        double fillPrice = (request.direction == Direction::BUY) ? quote.ask : quote.bid;
        int64_t filledQty = std::max(
            int64_t{1},
            static_cast<int64_t>(request.quantity * scenario.partialFillRatio)
        );
        
        OrderResult result;
        result.orderId = request.orderId;
        result.status = (filledQty < request.quantity) 
            ? Status::PARTIALLY_FILLED 
            : Status::FILLED;
        result.executedPrice = fillPrice;
        result.executedQuantity = filledQty;
        result.message = "Partial fill: " + std::to_string(filledQty) + "/" + std::to_string(request.quantity);
        
        return result;
    }
    
    OrderResult queueForDelayedFill(const OrderRequest& request, 
                                    std::chrono::milliseconds delay) {
        // LIMIT с хорошей ценой исполняем сразу
        if (request.type == Type::LIMIT) {
            auto quoteOpt = priceSimulator_->getQuote(request.figi);
            if (quoteOpt) {
                const auto& quote = *quoteOpt;
                bool canFillNow = false;
                double fillPrice = 0.0;
                
                if (request.direction == Direction::BUY && quote.ask <= request.price) {
                    canFillNow = true;
                    fillPrice = quote.ask;
                } else if (request.direction == Direction::SELL && quote.bid >= request.price) {
                    canFillNow = true;
                    fillPrice = quote.bid;
                }
                
                if (canFillNow) {
                    // Исполняем сразу, не ждём
                    OrderResult result;
                    result.orderId = request.orderId;
                    result.status = Status::FILLED;
                    result.executedPrice = fillPrice;
                    result.executedQuantity = request.quantity;
                    result.message = "Limit order filled immediately (price matched)";
                    return result;
                }
            }
        }
        
        // Ставим в очередь
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        
        PendingOrder pending;
        pending.orderId = request.orderId;
        pending.accountId = request.accountId;
        pending.figi = request.figi;
        pending.direction = request.direction;
        pending.type = request.type;
        pending.quantity = request.quantity;
        pending.limitPrice = request.price;
        pending.createdAt = now;
        pending.fillAfter = now + delay;
        // TRUE только для MARKET!
        pending.isDelayedMarket = (request.type == Type::MARKET);
        
        pendingOrders_[pending.orderId] = pending;
        
        OrderResult result;
        result.orderId = pending.orderId;
        result.status = Status::PENDING;
        result.message = pending.isDelayedMarket 
            ? "Market order queued for delayed fill"
            : "Limit order queued (waiting for price)";
        
        return result;
    }
    
    OrderResult queueLimitOrder(const OrderRequest& request) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        PendingOrder pending;
        pending.orderId = request.orderId;
        pending.accountId = request.accountId;
        pending.figi = request.figi;
        pending.direction = request.direction;
        pending.type = request.type;
        pending.quantity = request.quantity;
        pending.limitPrice = request.price;
        pending.createdAt = std::chrono::system_clock::now();
        
        pendingOrders_[pending.orderId] = pending;
        
        OrderResult result;
        result.orderId = pending.orderId;
        result.status = Status::PENDING;
        result.message = "Limit order queued";
        
        return result;
    }
};

} // namespace broker::adapters::secondary
