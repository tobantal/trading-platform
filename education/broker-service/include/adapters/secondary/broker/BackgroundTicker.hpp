#pragma once

#include "PriceSimulator.hpp"
#include "OrderProcessor.hpp"
#include "MarketScenario.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace broker::adapters::secondary {

/**
 * @brief Callback при изменении котировки
 */
struct QuoteUpdate {
    std::string figi;
    double bid;
    double ask;
    double last;
    int64_t volume;
};

using QuoteUpdateCallback = std::function<void(const QuoteUpdate&)>;

/**
 * @brief Фоновый поток для симуляции рынка
 */
class BackgroundTicker {
public:
    BackgroundTicker(
        std::shared_ptr<PriceSimulator> priceSimulator,
        std::shared_ptr<OrderProcessor> orderProcessor = nullptr,
        std::chrono::milliseconds interval = std::chrono::milliseconds{100})
        : priceSimulator_(std::move(priceSimulator))
        , orderProcessor_(std::move(orderProcessor))
        , interval_(interval)
        , running_(false)
        , tickCount_(0)
    {}
    
    ~BackgroundTicker() {
        stop();
    }
    
    // Non-copyable, non-movable
    BackgroundTicker(const BackgroundTicker&) = delete;
    BackgroundTicker& operator=(const BackgroundTicker&) = delete;
    
    void addInstrument(const std::string& figi) {
        std::lock_guard<std::mutex> lock(mutex_);
        instruments_.push_back(figi);
    }
    
    void removeInstrument(const std::string& figi) {
        std::lock_guard<std::mutex> lock(mutex_);
        instruments_.erase(
            std::remove(instruments_.begin(), instruments_.end(), figi),
            instruments_.end());
    }
    
    void setQuoteCallback(QuoteUpdateCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        quoteCallback_ = std::move(callback);
    }
    
    void setInterval(std::chrono::milliseconds interval) {
        interval_ = interval;
    }
    
    void start() {
        if (running_.exchange(true)) return;
        
        thread_ = std::thread([this]() {
            while (running_) {
                doTick();
                std::this_thread::sleep_for(interval_);
            }
        });
    }
    
    void stop() {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
    }
    
    bool isRunning() const { return running_; }
    
    uint64_t getTickCount() const { return tickCount_; }
    
    uint64_t tickCount() const { return tickCount_; }
    
    /**
     * @brief Выполнить один тик вручную (для тестов)
     */
    void manualTick() {
        doTick();
    }

private:
    std::shared_ptr<PriceSimulator> priceSimulator_;
    std::shared_ptr<OrderProcessor> orderProcessor_;
    std::chrono::milliseconds interval_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> tickCount_;
    std::thread thread_;
    mutable std::mutex mutex_;
    std::vector<std::string> instruments_;
    QuoteUpdateCallback quoteCallback_;
    MarketScenario defaultScenario_ = MarketScenario::realistic();
    
    void doTick() {
        std::vector<std::string> currentInstruments;
        QuoteUpdateCallback currentCallback;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            currentInstruments = instruments_;
            currentCallback = quoteCallback_;
        }
        
        // 1. Tick all instruments
        for (const auto& figi : currentInstruments) {
            priceSimulator_->tick(figi);
            
            // 2. Notify about quote update
            if (currentCallback) {
                auto quote = priceSimulator_->getQuote(figi);
                if (quote) {
                    QuoteUpdate update;
                    update.figi = figi;
                    update.bid = quote->bid;
                    update.ask = quote->ask;
                    update.last = quote->last;
                    update.volume = quote->volume;
                    
                    currentCallback(update);
                }
            }
        }
        
        // 3. Process pending orders
        if (orderProcessor_) {
            orderProcessor_->processPendingOrders(defaultScenario_);
        }
        
        ++tickCount_;
    }
};

} // namespace broker::adapters::secondary
