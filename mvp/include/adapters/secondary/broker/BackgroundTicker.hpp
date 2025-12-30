#pragma once

#include "PriceSimulator.hpp"
#include "OrderProcessor.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace trading::adapters::secondary {

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
 * 
 * Периодически:
 * - Вызывает tick() для всех инструментов (изменение цен)
 * - Обрабатывает pending limit-ордера
 * - Публикует события QuoteUpdated
 * 
 * @example
 * ```cpp
 * auto priceSimulator = std::make_shared<PriceSimulator>();
 * auto orderProcessor = std::make_shared<OrderProcessor>(priceSimulator);
 * 
 * BackgroundTicker ticker(priceSimulator, orderProcessor);
 * ticker.addInstrument("SBER");
 * ticker.setQuoteCallback([](const QuoteUpdate& q) {
 *     eventBus->publish(QuoteUpdatedEvent(q));
 * });
 * 
 * ticker.start(100ms);  // Тик каждые 100мс
 * // ... работа с брокером ...
 * ticker.stop();
 * ```
 * 
 * Thread-safe: да
 */
class BackgroundTicker {
public:
    /**
     * @brief Конструктор
     * @param priceSimulator Симулятор цен
     * @param orderProcessor Процессор ордеров (опционально)
     */
    BackgroundTicker(
        std::shared_ptr<PriceSimulator> priceSimulator,
        std::shared_ptr<OrderProcessor> orderProcessor = nullptr)
        : priceSimulator_(std::move(priceSimulator))
        , orderProcessor_(std::move(orderProcessor))
        , running_(false)
        , tickCount_(0)
    {}
    
    ~BackgroundTicker() {
        stop();
    }
    
    // Non-copyable, non-movable
    BackgroundTicker(const BackgroundTicker&) = delete;
    BackgroundTicker& operator=(const BackgroundTicker&) = delete;
    
    /**
     * @brief Добавить инструмент для симуляции
     */
    void addInstrument(const std::string& figi) {
        std::lock_guard<std::mutex> lock(mutex_);
        instruments_.push_back(figi);
    }
    
    /**
     * @brief Удалить инструмент
     */
    void removeInstrument(const std::string& figi) {
        std::lock_guard<std::mutex> lock(mutex_);
        instruments_.erase(
            std::remove(instruments_.begin(), instruments_.end(), figi),
            instruments_.end());
    }
    
    /**
     * @brief Установить callback для обновления котировок
     */
    void setQuoteCallback(QuoteUpdateCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        quoteCallback_ = std::move(callback);
    }
    
    /**
     * @brief Запустить фоновую симуляцию
     * @param interval Интервал между тиками
     */
    void start(std::chrono::milliseconds interval = std::chrono::milliseconds{100}) {
        if (running_.exchange(true)) {
            return;  // Уже запущен
        }
        
        tickInterval_ = interval;
        
        workerThread_ = std::thread([this]() {
            runLoop();
        });
    }
    
    /**
     * @brief Остановить симуляцию
     */
    void stop() {
        if (!running_.exchange(false)) {
            return;  // Уже остановлен
        }
        
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
    }
    
    /**
     * @brief Проверить, запущен ли тикер
     */
    bool isRunning() const {
        return running_.load();
    }
    
    /**
     * @brief Получить количество выполненных тиков
     */
    uint64_t tickCount() const {
        return tickCount_.load();
    }
    
    /**
     * @brief Выполнить один тик вручную (для тестов)
     */
    void manualTick() {
        doTick();
    }
    
    /**
     * @brief Установить интервал тиков (можно менять на лету)
     */
    void setInterval(std::chrono::milliseconds interval) {
        tickInterval_ = interval;
    }
    
    /**
     * @brief Получить текущий интервал
     */
    std::chrono::milliseconds interval() const {
        return tickInterval_.load();
    }

private:
    std::shared_ptr<PriceSimulator> priceSimulator_;
    std::shared_ptr<OrderProcessor> orderProcessor_;
    
    std::atomic<bool> running_;
    std::thread workerThread_;
    std::atomic<std::chrono::milliseconds> tickInterval_{std::chrono::milliseconds{100}};
    std::atomic<uint64_t> tickCount_;
    
    mutable std::mutex mutex_;
    std::vector<std::string> instruments_;
    QuoteUpdateCallback quoteCallback_;
    
    void runLoop() {
        while (running_.load()) {
            auto start = std::chrono::steady_clock::now();
            
            doTick();
            
            auto elapsed = std::chrono::steady_clock::now() - start;
            auto sleepTime = tickInterval_.load() - 
                std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
            
            if (sleepTime.count() > 0) {
                std::this_thread::sleep_for(sleepTime);
            }
        }
    }
    
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
            orderProcessor_->processPendingOrders();
        }
        
        ++tickCount_;
    }
};

} // namespace trading::adapters::secondary
