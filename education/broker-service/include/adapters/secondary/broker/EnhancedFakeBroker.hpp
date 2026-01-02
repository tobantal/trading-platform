#pragma once

#include "MarketScenario.hpp"
#include "PriceSimulator.hpp"
#include "OrderProcessor.hpp"
#include "BackgroundTicker.hpp"

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace broker::adapters::secondary {

// ============================================================================
// DOMAIN TYPES (Ð´Ð»Ñ Ð¸Ð·Ð¾Ð»ÑÑ†Ð¸Ð¸ Ð¾Ñ‚ Ð¾ÑÐ½Ð¾Ð²Ð½Ð¾Ð³Ð¾ domain)
// ============================================================================

/**
 * @brief ÐšÐ¾Ñ‚Ð¸Ñ€Ð¾Ð²ÐºÐ° (ÑÐ¾Ð²Ð¼ÐµÑÑ‚Ð¸Ð¼Ð°Ñ Ñ domain::Quote)
 */
struct BrokerQuote {
    std::string figi;
    std::string ticker;
    double lastPrice = 0.0;
    double bidPrice = 0.0;
    double askPrice = 0.0;
    int64_t volume = 0;
    
    double spread() const { return askPrice - bidPrice; }
    double mid() const { return (bidPrice + askPrice) / 2.0; }
};

/**
 * @brief Ð˜Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚
 */
struct BrokerInstrument {
    std::string figi;
    std::string ticker;
    std::string name;
    std::string currency = "RUB";
    int lot = 1;
    double minPriceIncrement = 0.01;
};

/**
 * @brief Ð—Ð°Ð¿Ñ€Ð¾Ñ Ð½Ð° Ð¾Ñ€Ð´ÐµÑ€ (ÑÐ¾Ð²Ð¼ÐµÑÑ‚Ð¸Ð¼Ñ‹Ð¹ Ñ domain::OrderRequest)
 */
struct BrokerOrderRequest {
    std::string accountId;
    std::string figi;
    Direction direction = Direction::BUY;
    Type type = Type::MARKET;
    int64_t quantity = 0;
    double price = 0.0;
};

/**
 * @brief Ð ÐµÐ·ÑƒÐ»ÑŒÑ‚Ð°Ñ‚ Ð¾Ñ€Ð´ÐµÑ€Ð° (ÑÐ¾Ð²Ð¼ÐµÑÑ‚Ð¸Ð¼Ñ‹Ð¹ Ñ domain::OrderResult)
 */
struct BrokerOrderResult {
    std::string orderId;
    Status status = Status::PENDING;
    double executedPrice = 0.0;
    int64_t executedQuantity = 0;
    std::string message;
    
    bool isSuccess() const {
        return status == Status::FILLED || status == Status::PARTIALLY_FILLED;
    }
};

/**
 * @brief ÐŸÐ¾Ð·Ð¸Ñ†Ð¸Ñ
 */
struct BrokerPosition {
    std::string figi;
    std::string ticker;
    int64_t quantity = 0;
    double averagePrice = 0.0;
    double currentPrice = 0.0;
    
    double totalValue() const { return quantity * currentPrice; }
    double pnl() const { return quantity * (currentPrice - averagePrice); }
    double pnlPercent() const {
        if (averagePrice == 0) return 0;
        return (currentPrice - averagePrice) / averagePrice * 100.0;
    }
};

/**
 * @brief ÐŸÐ¾Ñ€Ñ‚Ñ„ÐµÐ»ÑŒ
 */
struct BrokerPortfolio {
    std::string accountId;
    double cash = 0.0;
    std::string currency = "RUB";
    std::vector<BrokerPosition> positions;
    
    double totalValue() const {
        double total = cash;
        for (const auto& pos : positions) {
            total += pos.totalValue();
        }
        return total;
    }
};

// ============================================================================
// CALLBACKS
// ============================================================================

/**
 * @brief Ð¡Ð¾Ð±Ñ‹Ñ‚Ð¸Ðµ Ð¸ÑÐ¿Ð¾Ð»Ð½ÐµÐ½Ð¸Ñ Ð¾Ñ€Ð´ÐµÑ€Ð°
 */
struct BrokerOrderFillEvent {
    std::string orderId;
    std::string accountId;
    std::string figi;
    Direction direction;
    int64_t quantity;
    double price;
    bool partial;
};

/**
 * @brief Ð¡Ð¾Ð±Ñ‹Ñ‚Ð¸Ðµ Ð¾Ð±Ð½Ð¾Ð²Ð»ÐµÐ½Ð¸Ñ ÐºÐ¾Ñ‚Ð¸Ñ€Ð¾Ð²ÐºÐ¸
 */
struct BrokerQuoteUpdateEvent {
    std::string figi;
    double bid;
    double ask;
    double last;
    int64_t volume;
};

using OrderFillEventCallback = std::function<void(const BrokerOrderFillEvent&)>;
using QuoteUpdateEventCallback = std::function<void(const BrokerQuoteUpdateEvent&)>;

// ============================================================================
// ENHANCED FAKE BROKER
// ============================================================================

/**
 * @brief Ð£Ð»ÑƒÑ‡ÑˆÐµÐ½Ð½Ñ‹Ð¹ Ñ„ÐµÐ¹ÐºÐ¾Ð²Ñ‹Ð¹ Ð±Ñ€Ð¾ÐºÐµÑ€ Ñ Ñ€ÐµÐ°Ð»Ð¸ÑÑ‚Ð¸Ñ‡Ð½Ð¾Ð¹ ÑÐ¸Ð¼ÑƒÐ»ÑÑ†Ð¸ÐµÐ¹
 * 
 * ÐžÐ±ÑŠÐµÐ´Ð¸Ð½ÑÐµÑ‚:
 * - PriceSimulator: Ð³ÐµÐ½ÐµÑ€Ð°Ñ†Ð¸Ñ Ñ†ÐµÐ½ Ñ random walk
 * - OrderProcessor: Ð¾Ð±Ñ€Ð°Ð±Ð¾Ñ‚ÐºÐ° Ð¾Ñ€Ð´ÐµÑ€Ð¾Ð² ÑÐ¾ ÑÑ†ÐµÐ½Ð°Ñ€Ð¸ÑÐ¼Ð¸
 * - BackgroundTicker: Ñ„Ð¾Ð½Ð¾Ð²Ð¾Ðµ Ð¾Ð±Ð½Ð¾Ð²Ð»ÐµÐ½Ð¸Ðµ Ñ†ÐµÐ½
 * 
 * ÐŸÐ¾Ð´Ð´ÐµÑ€Ð¶Ð¸Ð²Ð°ÐµÑ‚:
 * - ÐœÐ½Ð¾Ð¶ÐµÑÑ‚Ð²Ð¾ Ð°ÐºÐºÐ°ÑƒÐ½Ñ‚Ð¾Ð² Ñ Ð¸Ð·Ð¾Ð»Ð¸Ñ€Ð¾Ð²Ð°Ð½Ð½Ñ‹Ð¼Ð¸ Ð¿Ð¾Ñ€Ñ‚Ñ„ÐµÐ»ÑÐ¼Ð¸
 * - Ð Ð°Ð·Ð»Ð¸Ñ‡Ð½Ñ‹Ðµ ÑÑ†ÐµÐ½Ð°Ñ€Ð¸Ð¸ Ð¸ÑÐ¿Ð¾Ð»Ð½ÐµÐ½Ð¸Ñ (slippage, partial fills, rejection)
 * - Ð¡Ð¾Ð±Ñ‹Ñ‚Ð¸Ñ Ð´Ð»Ñ Ð¸Ð½Ñ‚ÐµÐ³Ñ€Ð°Ñ†Ð¸Ð¸ Ñ EventBus
 * - Ð”ÐµÑ‚ÐµÑ€Ð¼Ð¸Ð½Ð¸Ñ€Ð¾Ð²Ð°Ð½Ð½Ð¾Ðµ Ñ‚ÐµÑÑ‚Ð¸Ñ€Ð¾Ð²Ð°Ð½Ð¸Ðµ (Ñ‡ÐµÑ€ÐµÐ· setPrice)
 * 
 * @example
 * ```cpp
 * EnhancedFakeBroker broker;
 * 
 * // ÐÐ°ÑÑ‚Ñ€Ð¾Ð¹ÐºÐ° ÑÑ†ÐµÐ½Ð°Ñ€Ð¸Ñ
 * broker.setScenario("SBER", MarketScenario::realistic(280.0));
 * 
 * // Ð ÐµÐ³Ð¸ÑÑ‚Ñ€Ð°Ñ†Ð¸Ñ Ð°ÐºÐºÐ°ÑƒÐ½Ñ‚Ð°
 * broker.registerAccount("acc-001", "token", 1000000.0);
 * 
 * // ÐŸÐ¾Ð´Ð¿Ð¸ÑÐºÐ° Ð½Ð° ÑÐ¾Ð±Ñ‹Ñ‚Ð¸Ñ
 * broker.setOrderFillCallback([&](const auto& e) {
 *     eventBus->publish(OrderFilledEvent(e));
 * });
 * 
 * // Ð—Ð°Ð¿ÑƒÑÐº ÑÐ¸Ð¼ÑƒÐ»ÑÑ†Ð¸Ð¸
 * broker.startSimulation(100ms);
 * 
 * // Ð Ð°Ð·Ð¼ÐµÑ‰ÐµÐ½Ð¸Ðµ Ð¾Ñ€Ð´ÐµÑ€Ð°
 * BrokerOrderRequest req{...};
 * auto result = broker.placeOrder("acc-001", req);
 * ```
 */
class EnhancedFakeBroker {
public:
    /**
     * @brief ÐšÐ¾Ð½ÑÑ‚Ñ€ÑƒÐºÑ‚Ð¾Ñ€
     * @param seed Seed Ð´Ð»Ñ RNG (0 = random)
     */
    explicit EnhancedFakeBroker(unsigned int seed = 0)
        : priceSimulator_(std::make_shared<PriceSimulator>(seed))
        , orderProcessor_(std::make_shared<OrderProcessor>(priceSimulator_))
        , ticker_(std::make_shared<BackgroundTicker>(priceSimulator_, orderProcessor_))
    {
        initDefaultInstruments();
        setupCallbacks();
    }
    
    ~EnhancedFakeBroker() {
        stopSimulation();
    }
    
    // ========================================================================
    // SIMULATION CONTROL
    // ========================================================================
    
    /**
     * @brief Ð—Ð°Ð¿ÑƒÑÑ‚Ð¸Ñ‚ÑŒ Ñ„Ð¾Ð½Ð¾Ð²ÑƒÑŽ ÑÐ¸Ð¼ÑƒÐ»ÑÑ†Ð¸ÑŽ Ñ†ÐµÐ½
     */
    void startSimulation(std::chrono::milliseconds interval = std::chrono::milliseconds{100}) {
        ticker_->setInterval(interval);
        ticker_->start();
    }
    
    /**
     * @brief ÐžÑÑ‚Ð°Ð½Ð¾Ð²Ð¸Ñ‚ÑŒ ÑÐ¸Ð¼ÑƒÐ»ÑÑ†Ð¸ÑŽ
     */
    void stopSimulation() {
        ticker_->stop();
    }
    
    /**
     * @brief ÐŸÑ€Ð¾Ð²ÐµÑ€Ð¸Ñ‚ÑŒ, Ð·Ð°Ð¿ÑƒÑ‰ÐµÐ½Ð° Ð»Ð¸ ÑÐ¸Ð¼ÑƒÐ»ÑÑ†Ð¸Ñ
     */
    bool isSimulationRunning() const {
        return ticker_->isRunning();
    }
    
    /**
     * @brief Ð’Ñ‹Ð¿Ð¾Ð»Ð½Ð¸Ñ‚ÑŒ Ð¾Ð´Ð¸Ð½ Ñ‚Ð¸Ðº Ð²Ñ€ÑƒÑ‡Ð½ÑƒÑŽ
     */
    void manualTick() {
        ticker_->manualTick();
    }
    
    // ========================================================================
    // SCENARIO CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Ð£ÑÑ‚Ð°Ð½Ð¾Ð²Ð¸Ñ‚ÑŒ ÑÑ†ÐµÐ½Ð°Ñ€Ð¸Ð¹ Ð´Ð»Ñ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð°
     */
    void setScenario(const std::string& figi, const MarketScenario& scenario) {
        std::lock_guard<std::mutex> lock(mutex_);
        scenarios_[figi] = scenario;
        priceSimulator_->initInstrument(
            figi,
            scenario.basePrice,
            scenario.bidAskSpread,
            scenario.volatility);
        ticker_->addInstrument(figi);
    }
    
    /**
     * @brief Ð£ÑÑ‚Ð°Ð½Ð¾Ð²Ð¸Ñ‚ÑŒ ÑÑ†ÐµÐ½Ð°Ñ€Ð¸Ð¹ Ð¿Ð¾ ÑƒÐ¼Ð¾Ð»Ñ‡Ð°Ð½Ð¸ÑŽ
     */
    void setDefaultScenario(const MarketScenario& scenario) {
        std::lock_guard<std::mutex> lock(mutex_);
        defaultScenario_ = scenario;
    }
    
    /**
     * @brief ÐŸÐ¾Ð»ÑƒÑ‡Ð¸Ñ‚ÑŒ ÑÑ†ÐµÐ½Ð°Ñ€Ð¸Ð¹ Ð´Ð»Ñ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð°
     */
    MarketScenario getScenario(const std::string& figi) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = scenarios_.find(figi);
        return (it != scenarios_.end()) ? it->second : defaultScenario_;
    }
    
    // ========================================================================
    // PRICE MANIPULATION (Ð´Ð»Ñ Ñ‚ÐµÑÑ‚Ð¾Ð²)
    // ========================================================================
    
    /**
     * @brief Ð£ÑÑ‚Ð°Ð½Ð¾Ð²Ð¸Ñ‚ÑŒ ÐºÐ¾Ð½ÐºÑ€ÐµÑ‚Ð½ÑƒÑŽ Ñ†ÐµÐ½Ñƒ
     */
    void setPrice(const std::string& figi, double price) {
        priceSimulator_->setPrice(figi, price);
    }
    
    /**
     * @brief Ð¡Ð´Ð²Ð¸Ð½ÑƒÑ‚ÑŒ Ñ†ÐµÐ½Ñƒ
     */
    void movePrice(const std::string& figi, double delta) {
        priceSimulator_->movePrice(figi, delta);
    }
    
    /**
     * @brief Ð˜Ð·Ð¼ÐµÐ½Ð¸Ñ‚ÑŒ Ñ†ÐµÐ½Ñƒ Ð½Ð° Ð¿Ñ€Ð¾Ñ†ÐµÐ½Ñ‚
     */
    void movePricePercent(const std::string& figi, double percent) {
        priceSimulator_->movePricePercent(figi, percent);
    }
    
    /**
     * @brief ÐŸÐ¾Ð»ÑƒÑ‡Ð¸Ñ‚ÑŒ ÑÐ¸Ð¼ÑƒÐ»ÑÑ‚Ð¾Ñ€ Ñ†ÐµÐ½ (Ð´Ð»Ñ Ð¿Ñ€ÑÐ¼Ð¾Ð³Ð¾ ÑƒÐ¿Ñ€Ð°Ð²Ð»ÐµÐ½Ð¸Ñ)
     */
    PriceSimulator& priceSimulator() { return *priceSimulator_; }
    
    /**
     * @brief ÐŸÐ¾Ð»ÑƒÑ‡Ð¸Ñ‚ÑŒ Ð¿Ñ€Ð¾Ñ†ÐµÑÑÐ¾Ñ€ Ð¾Ñ€Ð´ÐµÑ€Ð¾Ð² (Ð´Ð»Ñ Ñ‚ÐµÑÑ‚Ð¾Ð²)
     */
    OrderProcessor& orderProcessor() { return *orderProcessor_; }
    
    // ========================================================================
    // EVENT CALLBACKS
    // ========================================================================
    
    /**
     * @brief Ð£ÑÑ‚Ð°Ð½Ð¾Ð²Ð¸Ñ‚ÑŒ callback Ð´Ð»Ñ ÑÐ¾Ð±Ñ‹Ñ‚Ð¸Ð¹ Ð¸ÑÐ¿Ð¾Ð»Ð½ÐµÐ½Ð¸Ñ Ð¾Ñ€Ð´ÐµÑ€Ð¾Ð²
     */
    void setOrderFillCallback(OrderFillEventCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        orderFillCallback_ = std::move(callback);
    }
    
    /**
     * @brief Ð£ÑÑ‚Ð°Ð½Ð¾Ð²Ð¸Ñ‚ÑŒ callback Ð´Ð»Ñ Ð¾Ð±Ð½Ð¾Ð²Ð»ÐµÐ½Ð¸Ñ ÐºÐ¾Ñ‚Ð¸Ñ€Ð¾Ð²Ð¾Ðº
     */
    void setQuoteUpdateCallback(QuoteUpdateEventCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        quoteUpdateCallback_ = std::move(callback);
        
        // ÐŸÐµÑ€ÐµÐ½Ð°ÑÑ‚Ñ€Ð°Ð¸Ð²Ð°ÐµÐ¼ ticker callback
        ticker_->setQuoteCallback([this](const QuoteUpdate& qu) {
            BrokerQuoteUpdateEvent event;
            event.figi = qu.figi;
            event.bid = qu.bid;
            event.ask = qu.ask;
            event.last = qu.last;
            event.volume = qu.volume;
            
            QuoteUpdateEventCallback cb;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                cb = quoteUpdateCallback_;
            }
            if (cb) {
                cb(event);
            }
        });
    }
    
    // ========================================================================
    // ACCOUNT MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Ð—Ð°Ñ€ÐµÐ³Ð¸ÑÑ‚Ñ€Ð¸Ñ€Ð¾Ð²Ð°Ñ‚ÑŒ Ð°ÐºÐºÐ°ÑƒÐ½Ñ‚
     */
    void registerAccount(const std::string& accountId, const std::string& token, double initialCash = 1000000.0) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        AccountData acc;
        acc.token = token;
        acc.cash = initialCash;
        accounts_[accountId] = acc;
    }
    
    /**
     * @brief Ð£Ð´Ð°Ð»Ð¸Ñ‚ÑŒ Ð°ÐºÐºÐ°ÑƒÐ½Ñ‚
     */
    void unregisterAccount(const std::string& accountId) {
        std::lock_guard<std::mutex> lock(mutex_);
        accounts_.erase(accountId);
    }
    
    /**
     * @brief ÐŸÑ€Ð¾Ð²ÐµÑ€Ð¸Ñ‚ÑŒ ÑÑƒÑ‰ÐµÑÑ‚Ð²Ð¾Ð²Ð°Ð½Ð¸Ðµ Ð°ÐºÐºÐ°ÑƒÐ½Ñ‚Ð°
     */
    bool hasAccount(const std::string& accountId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return accounts_.find(accountId) != accounts_.end();
    }
    
    /**
     * @brief Ð£ÑÑ‚Ð°Ð½Ð¾Ð²Ð¸Ñ‚ÑŒ Ð±Ð°Ð»Ð°Ð½Ñ Ð°ÐºÐºÐ°ÑƒÐ½Ñ‚Ð° (Ð´Ð»Ñ Ñ‚ÐµÑÑ‚Ð¾Ð²)
     */
    void setCash(const std::string& accountId, double cash) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = accounts_.find(accountId);
        if (it != accounts_.end()) {
            it->second.cash = cash;
        }
    }
    
    // ========================================================================
    // MARKET DATA
    // ========================================================================
    
    /**
     * @brief ÐŸÐ¾Ð»ÑƒÑ‡Ð¸Ñ‚ÑŒ ÐºÐ¾Ñ‚Ð¸Ñ€Ð¾Ð²ÐºÑƒ
     */
    std::optional<BrokerQuote> getQuote(const std::string& figi) const {
        auto quote = priceSimulator_->getQuote(figi);
        if (!quote) {
            return std::nullopt;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        auto instrIt = instruments_.find(figi);
        
        BrokerQuote result;
        result.figi = figi;
        result.ticker = (instrIt != instruments_.end()) ? instrIt->second.ticker : figi;
        result.lastPrice = quote->last;
        result.bidPrice = quote->bid;
        result.askPrice = quote->ask;
        result.volume = quote->volume;
        
        return result;
    }
    
    /**
     * @brief ÐŸÐ¾Ð»ÑƒÑ‡Ð¸Ñ‚ÑŒ ÐºÐ¾Ñ‚Ð¸Ñ€Ð¾Ð²ÐºÐ¸ Ð´Ð»Ñ Ð½ÐµÑÐºÐ¾Ð»ÑŒÐºÐ¸Ñ… Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð¾Ð²
     */
    std::vector<BrokerQuote> getQuotes(const std::vector<std::string>& figis) const {
        std::vector<BrokerQuote> result;
        for (const auto& figi : figis) {
            if (auto quote = getQuote(figi)) {
                result.push_back(*quote);
            }
        }
        return result;
    }
    
    /**
     * @brief ÐŸÐ¾Ð»ÑƒÑ‡Ð¸Ñ‚ÑŒ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚ Ð¿Ð¾ FIGI
     */
    std::optional<BrokerInstrument> getInstrument(const std::string& figi) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = instruments_.find(figi);
        if (it == instruments_.end()) {
            return std::nullopt;
        }
        return it->second;
    }
    
    /**
     * @brief ÐŸÐ¾Ð»ÑƒÑ‡Ð¸Ñ‚ÑŒ Ð²ÑÐµ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ñ‹
     */
    std::vector<BrokerInstrument> getAllInstruments() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<BrokerInstrument> result;
        for (const auto& [figi, instr] : instruments_) {
            result.push_back(instr);
        }
        return result;
    }
    
    /**
     * @brief ÐŸÐ¾Ð¸ÑÐº Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð¾Ð² Ð¿Ð¾ Ñ‚Ð¸ÐºÐµÑ€Ñƒ/Ð½Ð°Ð·Ð²Ð°Ð½Ð¸ÑŽ
     */
    std::vector<BrokerInstrument> searchInstruments(const std::string& query) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<BrokerInstrument> result;
        
        std::string q = query;
        std::transform(q.begin(), q.end(), q.begin(), ::tolower);
        
        for (const auto& [figi, instr] : instruments_) {
            std::string ticker = instr.ticker;
            std::transform(ticker.begin(), ticker.end(), ticker.begin(), ::tolower);
            std::string name = instr.name;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            
            if (ticker.find(q) != std::string::npos || name.find(q) != std::string::npos) {
                result.push_back(instr);
            }
        }
        return result;
    }
    
    // ========================================================================
    // PORTFOLIO
    // ========================================================================
    
    /**
     * @brief ÐŸÐ¾Ð»ÑƒÑ‡Ð¸Ñ‚ÑŒ Ð¿Ð¾Ñ€Ñ‚Ñ„ÐµÐ»ÑŒ Ð°ÐºÐºÐ°ÑƒÐ½Ñ‚Ð°
     */
    std::optional<BrokerPortfolio> getPortfolio(const std::string& accountId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = accounts_.find(accountId);
        if (it == accounts_.end()) {
            return std::nullopt;
        }
        
        BrokerPortfolio portfolio;
        portfolio.accountId = accountId;
        portfolio.cash = it->second.cash;
        portfolio.currency = "RUB";
        
        for (const auto& [figi, pos] : it->second.positions) {
            BrokerPosition bpos;
            bpos.figi = figi;
            bpos.ticker = pos.ticker;
            bpos.quantity = pos.quantity;
            bpos.averagePrice = pos.averagePrice;
            
            // ÐžÐ±Ð½Ð¾Ð²Ð»ÑÐµÐ¼ Ñ‚ÐµÐºÑƒÑ‰ÑƒÑŽ Ñ†ÐµÐ½Ñƒ
            auto quote = priceSimulator_->getQuote(figi);
            bpos.currentPrice = quote ? quote->last : pos.averagePrice;
            
            portfolio.positions.push_back(bpos);
        }
        
        return portfolio;
    }
    
    // ========================================================================
    // ORDERS
    // ========================================================================
    
    /**
     * @brief Ð Ð°Ð·Ð¼ÐµÑÑ‚Ð¸Ñ‚ÑŒ Ð¾Ñ€Ð´ÐµÑ€
     */
    BrokerOrderResult placeOrder(const std::string& accountId, const BrokerOrderRequest& request) {
        // 1. ÐŸÑ€Ð¾Ð²ÐµÑ€ÑÐµÐ¼ Ð°ÐºÐºÐ°ÑƒÐ½Ñ‚
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = accounts_.find(accountId);
            if (it == accounts_.end()) {
                BrokerOrderResult result;
                result.status = Status::REJECTED;
                result.message = "Account not found: " + accountId;
                return result;
            }
        }
        
        // 2. ÐŸÑ€Ð¾Ð²ÐµÑ€ÑÐµÐ¼ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚
        auto instrOpt = getInstrument(request.figi);
        if (!instrOpt) {
            BrokerOrderResult result;
            result.status = Status::REJECTED;
            result.message = "Instrument not found: " + request.figi;
            return result;
        }
        
        // 3. ÐŸÑ€Ð¾Ð²ÐµÑ€ÑÐµÐ¼ Ð´Ð¾ÑÑ‚Ð°Ñ‚Ð¾Ñ‡Ð½Ð¾ÑÑ‚ÑŒ ÑÑ€ÐµÐ´ÑÑ‚Ð²/Ð¿Ð¾Ð·Ð¸Ñ†Ð¸Ð¸
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto& account = accounts_[accountId];
            
            auto quote = priceSimulator_->getQuote(request.figi);
            double price = quote ? quote->ask : 0.0;
            double totalCost = price * request.quantity * instrOpt->lot;
            
            if (request.direction == Direction::BUY) {
                if (account.cash < totalCost) {
                    BrokerOrderResult result;
                    result.status = Status::REJECTED;
                    result.message = "Insufficient funds";
                    return result;
                }
            } else {
                int64_t requiredShares = request.quantity * instrOpt->lot;
                auto posIt = account.positions.find(request.figi);
                if (posIt == account.positions.end() || posIt->second.quantity < requiredShares) {
                    BrokerOrderResult result;
                    result.status = Status::REJECTED;
                    result.message = "Insufficient position";
                    return result;
                }
            }
        }
        
        // 4. ÐžÐ±Ñ€Ð°Ð±Ð°Ñ‚Ñ‹Ð²Ð°ÐµÐ¼ Ð¾Ñ€Ð´ÐµÑ€ Ñ‡ÐµÑ€ÐµÐ· OrderProcessor
        OrderRequest procRequest;
        procRequest.accountId = accountId;
        procRequest.figi = request.figi;
        procRequest.direction = request.direction;
        procRequest.type = request.type;
        procRequest.quantity = request.quantity;
        procRequest.price = request.price;
        
        auto scenario = getScenario(request.figi);
        auto procResult = orderProcessor_->processOrder(procRequest, scenario);
        
        // 5. Ð•ÑÐ»Ð¸ Ð¸ÑÐ¿Ð¾Ð»Ð½ÐµÐ½ â€” Ð¾Ð±Ð½Ð¾Ð²Ð»ÑÐµÐ¼ Ð¿Ð¾Ñ€Ñ‚Ñ„ÐµÐ»ÑŒ
        if (procResult.status == Status::FILLED || procResult.status == Status::PARTIALLY_FILLED) {
            executeOrder(accountId, request, *instrOpt, procResult);
        }
        
        // 6. ÐšÐ¾Ð½Ð²ÐµÑ€Ñ‚Ð¸Ñ€ÑƒÐµÐ¼ Ñ€ÐµÐ·ÑƒÐ»ÑŒÑ‚Ð°Ñ‚
        BrokerOrderResult result;
        result.orderId = procResult.orderId;
        result.status = procResult.status;
        result.executedPrice = procResult.executedPrice;
        result.executedQuantity = procResult.executedQuantity;
        result.message = procResult.message;
        
        return result;
    }
    
    /**
     * @brief ÐžÑ‚Ð¼ÐµÐ½Ð¸Ñ‚ÑŒ Ð¾Ñ€Ð´ÐµÑ€
     */
    bool cancelOrder(const std::string& accountId, const std::string& orderId) {
        return orderProcessor_->cancelOrder(orderId);
    }
    
    /**
     * @brief ÐŸÐ¾Ð»ÑƒÑ‡Ð¸Ñ‚ÑŒ ÐºÐ¾Ð»Ð¸Ñ‡ÐµÑÑ‚Ð²Ð¾ pending Ð¾Ñ€Ð´ÐµÑ€Ð¾Ð²
     */
    size_t pendingOrderCount() const {
        return orderProcessor_->pendingCount();
    }
    
    // ========================================================================
    // RESET (Ð´Ð»Ñ Ñ‚ÐµÑÑ‚Ð¾Ð²)
    // ========================================================================
    
    /**
     * @brief Ð¡Ð±Ñ€Ð¾ÑÐ¸Ñ‚ÑŒ ÑÐ¾ÑÑ‚Ð¾ÑÐ½Ð¸Ðµ Ð±Ñ€Ð¾ÐºÐµÑ€Ð°
     */
    void reset() {
        stopSimulation();
        
        std::lock_guard<std::mutex> lock(mutex_);
        accounts_.clear();
        scenarios_.clear();
        orderProcessor_->clearPending();
        priceSimulator_->clear();
        
        initDefaultInstruments();
    }

private:
    std::shared_ptr<PriceSimulator> priceSimulator_;
    std::shared_ptr<OrderProcessor> orderProcessor_;
    std::shared_ptr<BackgroundTicker> ticker_;
    
    mutable std::mutex mutex_;
    
    // Ð¡Ñ†ÐµÐ½Ð°Ñ€Ð¸Ð¸
    MarketScenario defaultScenario_;
    std::unordered_map<std::string, MarketScenario> scenarios_;
    
    // Ð˜Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ñ‹
    std::unordered_map<std::string, BrokerInstrument> instruments_;
    
    // ÐÐºÐºÐ°ÑƒÐ½Ñ‚Ñ‹
    struct PositionData {
        std::string ticker;
        int64_t quantity = 0;
        double averagePrice = 0.0;
    };
    
    struct AccountData {
        std::string token;
        double cash = 0.0;
        std::unordered_map<std::string, PositionData> positions;
    };
    
    std::unordered_map<std::string, AccountData> accounts_;
    
    // Callbacks
    OrderFillEventCallback orderFillCallback_;
    QuoteUpdateEventCallback quoteUpdateCallback_;
    
    void initDefaultInstruments() {
        // SBER
        instruments_["BBG004730N88"] = {"BBG004730N88", "SBER", "Ð¡Ð±ÐµÑ€Ð±Ð°Ð½Ðº", "RUB", 10, 0.01};
        priceSimulator_->initInstrument("BBG004730N88", 280.0, 0.001, 0.002);
        ticker_->addInstrument("BBG004730N88");
        
        // GAZP
        instruments_["BBG004730RP0"] = {"BBG004730RP0", "GAZP", "Ð“Ð°Ð·Ð¿Ñ€Ð¾Ð¼", "RUB", 10, 0.01};
        priceSimulator_->initInstrument("BBG004730RP0", 160.0, 0.001, 0.003);
        ticker_->addInstrument("BBG004730RP0");
        
        // YNDX
        instruments_["BBG006L8G4H1"] = {"BBG006L8G4H1", "YNDX", "Ð¯Ð½Ð´ÐµÐºÑ", "RUB", 1, 0.1};
        priceSimulator_->initInstrument("BBG006L8G4H1", 3500.0, 0.002, 0.004);
        ticker_->addInstrument("BBG006L8G4H1");
        
        // LKOH
        instruments_["BBG004731032"] = {"BBG004731032", "LKOH", "Ð›ÑƒÐºÐ¾Ð¹Ð»", "RUB", 1, 0.5};
        priceSimulator_->initInstrument("BBG004731032", 7200.0, 0.001, 0.002);
        ticker_->addInstrument("BBG004731032");
        
        // MGNT
        instruments_["BBG004RVFCY3"] = {"BBG004RVFCY3", "MGNT", "ÐœÐ°Ð³Ð½Ð¸Ñ‚", "RUB", 1, 0.5};
        priceSimulator_->initInstrument("BBG004RVFCY3", 5500.0, 0.002, 0.003);
        ticker_->addInstrument("BBG004RVFCY3");
    }
    
    void setupCallbacks() {
        // Callback Ð¾Ñ‚ OrderProcessor Ð¿Ñ€Ð¸ Ð¸ÑÐ¿Ð¾Ð»Ð½ÐµÐ½Ð¸Ð¸ pending Ð¾Ñ€Ð´ÐµÑ€Ð¾Ð²
        orderProcessor_->setFillCallback([this](const OrderFillEvent& e) {
            // ÐžÐ±Ð½Ð¾Ð²Ð»ÑÐµÐ¼ Ð¿Ð¾Ñ€Ñ‚Ñ„ÐµÐ»ÑŒ
            auto instrOpt = getInstrument(e.figi);
            if (instrOpt) {
                BrokerOrderRequest req;
                req.accountId = e.accountId;
                req.figi = e.figi;
                req.quantity = e.quantity;
                req.direction = e.direction;
                
                OrderResult procResult;
                procResult.orderId = e.orderId;
                procResult.executedPrice = e.price;
                procResult.executedQuantity = e.quantity;
                procResult.status = e.partial ? Status::PARTIALLY_FILLED : Status::FILLED;
                
                executeOrder(e.accountId, req, *instrOpt, procResult);
            }
            
            // Ð’Ñ‹Ð·Ñ‹Ð²Ð°ÐµÐ¼ Ð²Ð½ÐµÑˆÐ½Ð¸Ð¹ callback
            OrderFillEventCallback cb;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                cb = orderFillCallback_;
            }
            if (cb) {
                BrokerOrderFillEvent event;
                event.orderId = e.orderId;
                event.accountId = e.accountId;
                event.figi = e.figi;
                event.direction = e.direction;
                event.quantity = e.quantity;
                event.price = e.price;
                event.partial = e.partial;
                cb(event);
            }
        });
    }
    
    void executeOrder(
        const std::string& accountId,
        const BrokerOrderRequest& request,
        const BrokerInstrument& instrument,
        const OrderResult& result)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = accounts_.find(accountId);
        if (it == accounts_.end()) return;
        
        auto& account = it->second;
        int64_t totalShares = result.executedQuantity * instrument.lot;
        double totalCost = result.executedPrice * totalShares;
        
        if (request.direction == Direction::BUY) {
            account.cash -= totalCost;
            
            auto& pos = account.positions[request.figi];
            if (pos.quantity == 0) {
                pos.ticker = instrument.ticker;
                pos.averagePrice = result.executedPrice;
                pos.quantity = totalShares;
            } else {
                // Ð¡Ñ€ÐµÐ´Ð½ÑÑ Ñ†ÐµÐ½Ð°
                double newAvg = (pos.averagePrice * pos.quantity + 
                                result.executedPrice * totalShares) /
                               (pos.quantity + totalShares);
                pos.averagePrice = newAvg;
                pos.quantity += totalShares;
            }
        } else {
            account.cash += totalCost;
            
            auto posIt = account.positions.find(request.figi);
            if (posIt != account.positions.end()) {
                posIt->second.quantity -= totalShares;
                if (posIt->second.quantity <= 0) {
                    account.positions.erase(posIt);
                }
            }
        }
    }
};

} // namespace broker::adapters::secondary

