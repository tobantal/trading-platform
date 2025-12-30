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

namespace trading::adapters::secondary {

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
    std::chrono::system_clock::time_point createdAt;
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
 * - DELAYED: отложенное исполнение (TODO)
 * - ALWAYS_REJECT: всегда отклонять
 * 
 * Для limit-ордеров поддерживает очередь pending с периодической
 * проверкой через processPendingOrders().
 * 
 * @example
 * ```cpp
 * auto priceSimulator = std::make_shared<PriceSimulator>();
 * priceSimulator->initInstrument("SBER", 280.0);
 * 
 * OrderProcessor processor(priceSimulator);
 * 
 * MarketScenario scenario = MarketScenario::realistic(280.0);
 * 
 * OrderRequest req;
 * req.figi = "SBER";
 * req.direction = Direction::BUY;
 * req.type = Type::MARKET;
 * req.quantity = 10;
 * 
 * auto result = processor.processOrder(req, scenario);
 * // result.status == Status::FILLED
 * ```
 */
class OrderProcessor {
public:
    /**
     * @brief Конструктор
     * @param priceSimulator Симулятор цен (обязательный)
     */
    explicit OrderProcessor(std::shared_ptr<PriceSimulator> priceSimulator)
        : priceSimulator_(std::move(priceSimulator))
        , rng_(std::random_device{}())
    {}
    
    /**
     * @brief Установить callback для событий исполнения
     */
    void setFillCallback(FillCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        fillCallback_ = std::move(callback);
    }
    
    /**
     * @brief Обработать ордер согласно сценарию
     * 
     * @param request Запрос на ордер
     * @param scenario Сценарий рынка
     * @return Результат обработки
     */
    OrderResult processOrder(const OrderRequest& request, const MarketScenario& scenario) {
        // 1. Проверка rejection
        if (shouldReject(scenario)) {
            return rejectOrder(scenario.rejectReason.empty() 
                ? "Order rejected by scenario" 
                : scenario.rejectReason);
        }
        
        // 2. Получаем котировку
        auto quoteOpt = priceSimulator_->getQuote(request.figi);
        if (!quoteOpt) {
            return rejectOrder("Instrument not found: " + request.figi);
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
                return queueForDelayedFill(request);
                
            case OrderFillBehavior::ALWAYS_REJECT:
                return rejectOrder(scenario.rejectReason.empty() 
                    ? "Always reject mode" 
                    : scenario.rejectReason);
        }
        
        return rejectOrder("Unknown fill behavior");
    }
    
    /**
     * @brief Обработать pending ордера
     * 
     * Проверяет все limit-ордера в очереди и исполняет те,
     * для которых цена достигла лимита.
     * 
     * @return Количество исполненных ордеров
     */
    int processPendingOrders() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int filledCount = 0;
        
        for (auto it = pendingOrders_.begin(); it != pendingOrders_.end(); ) {
            auto& order = it->second;
            
            auto quoteOpt = priceSimulator_->getQuote(order.figi);
            if (!quoteOpt) {
                ++it;
                continue;
            }
            
            const auto& quote = *quoteOpt;
            
            bool shouldFill = false;
            double fillPrice = 0.0;
            
            if (order.type == Type::LIMIT) {
                if (order.direction == Direction::BUY) {
                    // Buy limit: исполняется когда ask <= limit price
                    shouldFill = quote.ask <= order.limitPrice;
                    fillPrice = std::min(quote.ask, order.limitPrice);
                } else {
                    // Sell limit: исполняется когда bid >= limit price
                    shouldFill = quote.bid >= order.limitPrice;
                    fillPrice = std::max(quote.bid, order.limitPrice);
                }
            }
            
            if (shouldFill) {
                // Публикуем событие
                publishFillEvent(order, fillPrice, false);
                
                it = pendingOrders_.erase(it);
                ++filledCount;
            } else {
                ++it;
            }
        }
        
        return filledCount;
    }
    
    /**
     * @brief Принудительно исполнить ордер (для тестов)
     * 
     * @param orderId ID ордера
     * @param price Цена исполнения
     * @return true если ордер найден и исполнен
     */
    bool forceFillOrder(const std::string& orderId, double price) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = pendingOrders_.find(orderId);
        if (it == pendingOrders_.end()) {
            return false;
        }
        
        publishFillEvent(it->second, price, false);
        pendingOrders_.erase(it);
        return true;
    }
    
    /**
     * @brief Принудительно отклонить ордер (для тестов)
     */
    bool forceRejectOrder(const std::string& orderId) {
        std::lock_guard<std::mutex> lock(mutex_);
        return pendingOrders_.erase(orderId) > 0;
    }
    
    /**
     * @brief Отменить ордер
     */
    bool cancelOrder(const std::string& orderId) {
        std::lock_guard<std::mutex> lock(mutex_);
        return pendingOrders_.erase(orderId) > 0;
    }
    
    /**
     * @brief Получить pending ордер
     */
    std::optional<PendingOrder> getPendingOrder(const std::string& orderId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = pendingOrders_.find(orderId);
        if (it == pendingOrders_.end()) {
            return std::nullopt;
        }
        return it->second;
    }
    
    /**
     * @brief Количество pending ордеров
     */
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
    
    std::string generateOrderId() {
        return "order-" + std::to_string(++orderCounter_);
    }
    
    bool shouldReject(const MarketScenario& scenario) {
        if (scenario.rejectProbability <= 0.0) return false;
        if (scenario.rejectProbability >= 1.0) return true;
        
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng_) < scenario.rejectProbability;
    }
    
    OrderResult rejectOrder(const std::string& reason) {
        OrderResult result;
        result.orderId = generateOrderId();
        result.status = Status::REJECTED;
        result.message = reason;
        return result;
    }
    
    OrderResult fillImmediately(const OrderRequest& request, const PriceSimulator::Quote& quote) {
        double fillPrice = (request.direction == Direction::BUY) ? quote.ask : quote.bid;
        
        OrderResult result;
        result.orderId = generateOrderId();
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
            result.orderId = generateOrderId();
            result.status = Status::FILLED;
            result.executedPrice = fillPrice;
            result.executedQuantity = request.quantity;
            result.message = slippage > 0 ? "Filled with slippage" : "Filled";
            
            return result;
            
        } else {
            // Limit order: проверяем можно ли исполнить сразу
            bool canFillNow = false;
            
            if (request.direction == Direction::BUY) {
                canFillNow = request.price >= quote.ask;
            } else {
                canFillNow = request.price <= quote.bid;
            }
            
            if (canFillNow) {
                return fillImmediately(request, quote);
            } else {
                // Добавляем в pending
                return addToPending(request);
            }
        }
    }
    
    OrderResult fillPartially(
        const OrderRequest& request,
        const PriceSimulator::Quote& quote,
        const MarketScenario& scenario)
    {
        int64_t filledQty = static_cast<int64_t>(request.quantity * scenario.partialFillRatio);
        filledQty = std::max(int64_t{1}, filledQty);
        
        double fillPrice = (request.direction == Direction::BUY) ? quote.ask : quote.bid;
        
        OrderResult result;
        result.orderId = generateOrderId();
        result.status = (filledQty < request.quantity) ? Status::PARTIALLY_FILLED : Status::FILLED;
        result.executedPrice = fillPrice;
        result.executedQuantity = filledQty;
        result.message = "Partially filled";
        
        return result;
    }
    
    OrderResult queueForDelayedFill(const OrderRequest& request) {
        // Для DELAYED пока просто ставим в очередь как pending
        return addToPending(request);
    }
    
    OrderResult addToPending(const OrderRequest& request) {
        std::string orderId = generateOrderId();
        
        PendingOrder pending;
        pending.orderId = orderId;
        pending.accountId = request.accountId;
        pending.figi = request.figi;
        pending.direction = request.direction;
        pending.type = request.type;
        pending.quantity = request.quantity;
        pending.limitPrice = request.price;
        pending.createdAt = std::chrono::system_clock::now();
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pendingOrders_[orderId] = pending;
        }
        
        OrderResult result;
        result.orderId = orderId;
        result.status = Status::PENDING;
        result.message = "Limit order queued";
        
        return result;
    }
    
    void publishFillEvent(const PendingOrder& order, double fillPrice, bool partial) {
        if (fillCallback_) {
            OrderFillEvent event;
            event.orderId = order.orderId;
            event.accountId = order.accountId;
            event.figi = order.figi;
            event.direction = order.direction;
            event.quantity = order.quantity;
            event.price = fillPrice;
            event.partial = partial;
            
            fillCallback_(event);
        }
    }
};

} // namespace trading::adapters::secondary
