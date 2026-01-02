#pragma once

#include <chrono>
#include <cmath>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>

namespace broker::adapters::secondary {

/**
 * @brief Ð¡Ð¸Ð¼ÑƒÐ»ÑÑ‚Ð¾Ñ€ Ñ€Ñ‹Ð½Ð¾Ñ‡Ð½Ñ‹Ñ… Ñ†ÐµÐ½
 * 
 * Ð“ÐµÐ½ÐµÑ€Ð¸Ñ€ÑƒÐµÑ‚ Ñ€ÐµÐ°Ð»Ð¸ÑÑ‚Ð¸Ñ‡Ð½Ñ‹Ðµ bid/ask Ñ†ÐµÐ½Ñ‹ Ñ:
 * - Random walk (ÑÐ»ÑƒÑ‡Ð°Ð¹Ð½Ð¾Ðµ Ð±Ð»ÑƒÐ¶Ð´Ð°Ð½Ð¸Ðµ)
 * - ÐÐ°ÑÑ‚Ñ€Ð°Ð¸Ð²Ð°ÐµÐ¼Ð¾Ð¹ Ð²Ð¾Ð»Ð°Ñ‚Ð¸Ð»ÑŒÐ½Ð¾ÑÑ‚ÑŒÑŽ
 * - Bid-ask ÑÐ¿Ñ€ÐµÐ´Ð¾Ð¼
 * 
 * Ð˜ÑÐ¿Ð¾Ð»ÑŒÐ·ÑƒÐµÑ‚ ÑƒÐ¿Ñ€Ð¾Ñ‰Ñ‘Ð½Ð½ÑƒÑŽ Ð¼Ð¾Ð´ÐµÐ»ÑŒ geometric Brownian motion:
 * dS = Ïƒ * S * dW
 * Ð³Ð´Ðµ Ïƒ - Ð²Ð¾Ð»Ð°Ñ‚Ð¸Ð»ÑŒÐ½Ð¾ÑÑ‚ÑŒ, dW - Ð²Ð¸Ð½ÐµÑ€Ð¾Ð²ÑÐºÐ¸Ð¹ Ð¿Ñ€Ð¾Ñ†ÐµÑÑ
 * 
 * @example
 * ```cpp
 * PriceSimulator sim;
 * sim.initInstrument("SBER", 280.0, 0.001, 0.002);
 * 
 * auto quote = sim.getQuote("SBER");
 * // quote.bid â‰ˆ 279.86
 * // quote.ask â‰ˆ 280.14
 * 
 * sim.tick("SBER");  // Ð¦ÐµÐ½Ð° Ð¸Ð·Ð¼ÐµÐ½Ð¸Ñ‚ÑÑ ÑÐ»ÑƒÑ‡Ð°Ð¹Ð½Ð¾
 * ```
 * 
 * Thread-safe: Ð´Ð° (Ð²Ð½ÑƒÑ‚Ñ€ÐµÐ½Ð½ÑÑ ÑÐ¸Ð½Ñ…Ñ€Ð¾Ð½Ð¸Ð·Ð°Ñ†Ð¸Ñ)
 */
class PriceSimulator {
public:
    /**
     * @brief ÐšÐ¾Ñ‚Ð¸Ñ€Ð¾Ð²ÐºÐ° Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð°
     */
    struct Quote {
        double bid = 0.0;           ///< Ð›ÑƒÑ‡ÑˆÐ°Ñ Ñ†ÐµÐ½Ð° Ð¿Ð¾ÐºÑƒÐ¿ÐºÐ¸
        double ask = 0.0;           ///< Ð›ÑƒÑ‡ÑˆÐ°Ñ Ñ†ÐµÐ½Ð° Ð¿Ñ€Ð¾Ð´Ð°Ð¶Ð¸
        double last = 0.0;          ///< ÐŸÐ¾ÑÐ»ÐµÐ´Ð½ÑÑ ÑÐ´ÐµÐ»ÐºÐ°
        int64_t volume = 0;         ///< ÐžÐ±ÑŠÑ‘Ð¼ Ð·Ð° Ð´ÐµÐ½ÑŒ
        std::chrono::system_clock::time_point timestamp;
        
        /**
         * @brief Ð¡Ñ€ÐµÐ´Ð½ÑÑ Ñ†ÐµÐ½Ð° (mid)
         */
        double mid() const {
            return (bid + ask) / 2.0;
        }
        
        /**
         * @brief Ð¡Ð¿Ñ€ÐµÐ´ Ð² Ð°Ð±ÑÐ¾Ð»ÑŽÑ‚Ð½Ñ‹Ñ… ÐµÐ´Ð¸Ð½Ð¸Ñ†Ð°Ñ…
         */
        double spreadAbs() const {
            return ask - bid;
        }
        
        /**
         * @brief Ð¡Ð¿Ñ€ÐµÐ´ Ð² Ð¿Ñ€Ð¾Ñ†ÐµÐ½Ñ‚Ð°Ñ…
         */
        double spreadPercent() const {
            if (mid() == 0.0) return 0.0;
            return spreadAbs() / mid() * 100.0;
        }
    };
    
    /**
     * @brief ÐšÐ¾Ð½ÑÑ‚Ñ€ÑƒÐºÑ‚Ð¾Ñ€
     * @param seed Seed Ð´Ð»Ñ Ð³ÐµÐ½ÐµÑ€Ð°Ñ‚Ð¾Ñ€Ð° ÑÐ»ÑƒÑ‡Ð°Ð¹Ð½Ñ‹Ñ… Ñ‡Ð¸ÑÐµÐ» (0 = random_device)
     */
    explicit PriceSimulator(unsigned int seed = 0)
        : rng_(seed == 0 ? std::random_device{}() : seed)
    {}
    
    /**
     * @brief Ð˜Ð½Ð¸Ñ†Ð¸Ð°Ð»Ð¸Ð·Ð¸Ñ€Ð¾Ð²Ð°Ñ‚ÑŒ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚
     * 
     * @param figi Ð˜Ð´ÐµÐ½Ñ‚Ð¸Ñ„Ð¸ÐºÐ°Ñ‚Ð¾Ñ€ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð°
     * @param basePrice ÐÐ°Ñ‡Ð°Ð»ÑŒÐ½Ð°Ñ Ñ†ÐµÐ½Ð°
     * @param spread Ð¡Ð¿Ñ€ÐµÐ´ bid/ask Ð² Ð´Ð¾Ð»ÑÑ… (0.001 = 0.1%)
     * @param volatility Ð’Ð¾Ð»Ð°Ñ‚Ð¸Ð»ÑŒÐ½Ð¾ÑÑ‚ÑŒ Ð·Ð° Ñ‚Ð¸Ðº Ð² Ð´Ð¾Ð»ÑÑ… (0.002 = 0.2%)
     */
    void initInstrument(
        const std::string& figi,
        double basePrice,
        double spread = 0.001,
        double volatility = 0.002)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        InstrumentState state;
        state.currentPrice = basePrice;
        state.spread = spread;
        state.volatility = volatility;
        state.dailyVolume = 1000000;
        state.lastUpdate = std::chrono::system_clock::now();
        
        instruments_[figi] = state;
    }
    
    /**
     * @brief ÐŸÑ€Ð¾Ð²ÐµÑ€Ð¸Ñ‚ÑŒ, Ð¸Ð½Ð¸Ñ†Ð¸Ð°Ð»Ð¸Ð·Ð¸Ñ€Ð¾Ð²Ð°Ð½ Ð»Ð¸ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚
     */
    bool hasInstrument(const std::string& figi) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return instruments_.find(figi) != instruments_.end();
    }
    
    /**
     * @brief ÐŸÐ¾Ð»ÑƒÑ‡Ð¸Ñ‚ÑŒ Ñ‚ÐµÐºÑƒÑ‰ÑƒÑŽ ÐºÐ¾Ñ‚Ð¸Ñ€Ð¾Ð²ÐºÑƒ
     * 
     * @param figi Ð˜Ð´ÐµÐ½Ñ‚Ð¸Ñ„Ð¸ÐºÐ°Ñ‚Ð¾Ñ€ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð°
     * @return ÐšÐ¾Ñ‚Ð¸Ñ€Ð¾Ð²ÐºÐ° Ð¸Ð»Ð¸ nullopt ÐµÑÐ»Ð¸ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚ Ð½Ðµ Ð½Ð°Ð¹Ð´ÐµÐ½
     */
    std::optional<Quote> getQuote(const std::string& figi) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = instruments_.find(figi);
        if (it == instruments_.end()) {
            return std::nullopt;
        }
        
        const auto& state = it->second;
        
        Quote q;
        q.last = state.currentPrice;
        q.bid = state.currentPrice * (1.0 - state.spread / 2.0);
        q.ask = state.currentPrice * (1.0 + state.spread / 2.0);
        q.volume = state.dailyVolume;
        q.timestamp = state.lastUpdate;
        
        return q;
    }
    
    /**
     * @brief Ð¡Ð¸Ð¼ÑƒÐ»Ð¸Ñ€Ð¾Ð²Ð°Ñ‚ÑŒ Ð¾Ð´Ð¸Ð½ Ñ‚Ð¸Ðº (Ð´Ð²Ð¸Ð¶ÐµÐ½Ð¸Ðµ Ñ†ÐµÐ½Ñ‹)
     * 
     * Ð˜ÑÐ¿Ð¾Ð»ÑŒÐ·ÑƒÐµÑ‚ geometric random walk:
     * P(t+1) = P(t) * (1 + Ïƒ * Z)
     * Ð³Ð´Ðµ Z ~ N(0, 1)
     * 
     * @param figi Ð˜Ð´ÐµÐ½Ñ‚Ð¸Ñ„Ð¸ÐºÐ°Ñ‚Ð¾Ñ€ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð°
     * @return ÐÐ¾Ð²Ð°Ñ Ñ†ÐµÐ½Ð° Ð¸Ð»Ð¸ 0.0 ÐµÑÐ»Ð¸ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚ Ð½Ðµ Ð½Ð°Ð¹Ð´ÐµÐ½
     */
    double tick(const std::string& figi) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = instruments_.find(figi);
        if (it == instruments_.end()) {
            return 0.0;
        }
        
        auto& state = it->second;
        
        // Random walk Ñ Ð½Ð¾Ñ€Ð¼Ð°Ð»ÑŒÐ½Ñ‹Ð¼ Ñ€Ð°ÑÐ¿Ñ€ÐµÐ´ÐµÐ»ÐµÐ½Ð¸ÐµÐ¼
        std::normal_distribution<double> dist(0.0, state.volatility);
        double change = dist(rng_);
        
        state.currentPrice *= (1.0 + change);
        state.currentPrice = std::max(0.01, state.currentPrice);  // ÐœÐ¸Ð½Ð¸Ð¼ÑƒÐ¼ 1 ÐºÐ¾Ð¿ÐµÐ¹ÐºÐ°
        state.lastUpdate = std::chrono::system_clock::now();
        
        return state.currentPrice;
    }
    
    /**
     * @brief Ð¡Ð¸Ð¼ÑƒÐ»Ð¸Ñ€Ð¾Ð²Ð°Ñ‚ÑŒ N Ñ‚Ð¸ÐºÐ¾Ð²
     * 
     * @param figi Ð˜Ð´ÐµÐ½Ñ‚Ð¸Ñ„Ð¸ÐºÐ°Ñ‚Ð¾Ñ€ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð°
     * @param ticks ÐšÐ¾Ð»Ð¸Ñ‡ÐµÑÑ‚Ð²Ð¾ Ñ‚Ð¸ÐºÐ¾Ð²
     * @return Ð¤Ð¸Ð½Ð°Ð»ÑŒÐ½Ð°Ñ Ñ†ÐµÐ½Ð°
     */
    double simulate(const std::string& figi, int ticks) {
        double price = 0.0;
        for (int i = 0; i < ticks; ++i) {
            price = tick(figi);
        }
        return price;
    }
    
    /**
     * @brief Ð£ÑÑ‚Ð°Ð½Ð¾Ð²Ð¸Ñ‚ÑŒ ÐºÐ¾Ð½ÐºÑ€ÐµÑ‚Ð½ÑƒÑŽ Ñ†ÐµÐ½Ñƒ (Ð´Ð»Ñ Ð´ÐµÑ‚ÐµÑ€Ð¼Ð¸Ð½Ð¸Ñ€Ð¾Ð²Ð°Ð½Ð½Ñ‹Ñ… Ñ‚ÐµÑÑ‚Ð¾Ð²)
     * 
     * @param figi Ð˜Ð´ÐµÐ½Ñ‚Ð¸Ñ„Ð¸ÐºÐ°Ñ‚Ð¾Ñ€ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð°
     * @param price ÐÐ¾Ð²Ð°Ñ Ñ†ÐµÐ½Ð°
     * @return true ÐµÑÐ»Ð¸ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚ Ð½Ð°Ð¹Ð´ÐµÐ½
     */
    bool setPrice(const std::string& figi, double price) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = instruments_.find(figi);
        if (it == instruments_.end()) {
            return false;
        }
        
        it->second.currentPrice = std::max(0.01, price);
        it->second.lastUpdate = std::chrono::system_clock::now();
        return true;
    }
    
    /**
     * @brief Ð¡Ð´Ð²Ð¸Ð½ÑƒÑ‚ÑŒ Ñ†ÐµÐ½Ñƒ Ð½Ð° delta
     * 
     * @param figi Ð˜Ð´ÐµÐ½Ñ‚Ð¸Ñ„Ð¸ÐºÐ°Ñ‚Ð¾Ñ€ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð°
     * @param delta Ð˜Ð·Ð¼ÐµÐ½ÐµÐ½Ð¸Ðµ Ñ†ÐµÐ½Ñ‹ (Ð¼Ð¾Ð¶ÐµÑ‚ Ð±Ñ‹Ñ‚ÑŒ Ð¾Ñ‚Ñ€Ð¸Ñ†Ð°Ñ‚ÐµÐ»ÑŒÐ½Ñ‹Ð¼)
     * @return ÐÐ¾Ð²Ð°Ñ Ñ†ÐµÐ½Ð° Ð¸Ð»Ð¸ 0.0 ÐµÑÐ»Ð¸ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚ Ð½Ðµ Ð½Ð°Ð¹Ð´ÐµÐ½
     */
    double movePrice(const std::string& figi, double delta) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = instruments_.find(figi);
        if (it == instruments_.end()) {
            return 0.0;
        }
        
        it->second.currentPrice = std::max(0.01, it->second.currentPrice + delta);
        it->second.lastUpdate = std::chrono::system_clock::now();
        return it->second.currentPrice;
    }
    
    /**
     * @brief Ð˜Ð·Ð¼ÐµÐ½Ð¸Ñ‚ÑŒ Ñ†ÐµÐ½Ñƒ Ð½Ð° Ð¿Ñ€Ð¾Ñ†ÐµÐ½Ñ‚
     * 
     * @param figi Ð˜Ð´ÐµÐ½Ñ‚Ð¸Ñ„Ð¸ÐºÐ°Ñ‚Ð¾Ñ€ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð°
     * @param percent Ð˜Ð·Ð¼ÐµÐ½ÐµÐ½Ð¸Ðµ Ð² Ð¿Ñ€Ð¾Ñ†ÐµÐ½Ñ‚Ð°Ñ… (-5.0 = -5%)
     * @return ÐÐ¾Ð²Ð°Ñ Ñ†ÐµÐ½Ð° Ð¸Ð»Ð¸ 0.0 ÐµÑÐ»Ð¸ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚ Ð½Ðµ Ð½Ð°Ð¹Ð´ÐµÐ½
     */
    double movePricePercent(const std::string& figi, double percent) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = instruments_.find(figi);
        if (it == instruments_.end()) {
            return 0.0;
        }
        
        it->second.currentPrice *= (1.0 + percent / 100.0);
        it->second.currentPrice = std::max(0.01, it->second.currentPrice);
        it->second.lastUpdate = std::chrono::system_clock::now();
        return it->second.currentPrice;
    }
    
    /**
     * @brief ÐŸÐ¾Ð»ÑƒÑ‡Ð¸Ñ‚ÑŒ Ñ‚ÐµÐºÑƒÑ‰ÑƒÑŽ Ñ†ÐµÐ½Ñƒ (Ð±ÐµÐ· bid/ask)
     */
    double getPrice(const std::string& figi) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = instruments_.find(figi);
        if (it == instruments_.end()) {
            return 0.0;
        }
        return it->second.currentPrice;
    }
    
    /**
     * @brief Ð˜Ð·Ð¼ÐµÐ½Ð¸Ñ‚ÑŒ Ð²Ð¾Ð»Ð°Ñ‚Ð¸Ð»ÑŒÐ½Ð¾ÑÑ‚ÑŒ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð°
     */
    bool setVolatility(const std::string& figi, double volatility) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = instruments_.find(figi);
        if (it == instruments_.end()) {
            return false;
        }
        
        it->second.volatility = std::max(0.0, volatility);
        return true;
    }
    
    /**
     * @brief Ð˜Ð·Ð¼ÐµÐ½Ð¸Ñ‚ÑŒ ÑÐ¿Ñ€ÐµÐ´ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð°
     */
    bool setSpread(const std::string& figi, double spread) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = instruments_.find(figi);
        if (it == instruments_.end()) {
            return false;
        }
        
        it->second.spread = std::max(0.0, spread);
        return true;
    }
    
    /**
     * @brief Ð£Ð´Ð°Ð»Ð¸Ñ‚ÑŒ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚
     */
    bool removeInstrument(const std::string& figi) {
        std::lock_guard<std::mutex> lock(mutex_);
        return instruments_.erase(figi) > 0;
    }
    
    /**
     * @brief ÐžÑ‡Ð¸ÑÑ‚Ð¸Ñ‚ÑŒ Ð²ÑÐµ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ñ‹
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        instruments_.clear();
    }
    
    /**
     * @brief ÐŸÐ¾Ð»ÑƒÑ‡Ð¸Ñ‚ÑŒ ÐºÐ¾Ð»Ð¸Ñ‡ÐµÑÑ‚Ð²Ð¾ Ð¸Ð½ÑÑ‚Ñ€ÑƒÐ¼ÐµÐ½Ñ‚Ð¾Ð²
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return instruments_.size();
    }

private:
    struct InstrumentState {
        double currentPrice = 100.0;
        double spread = 0.001;       // 0.1%
        double volatility = 0.002;   // 0.2% per tick
        int64_t dailyVolume = 1000000;
        std::chrono::system_clock::time_point lastUpdate;
    };
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, InstrumentState> instruments_;
    std::mt19937 rng_;
};

} // namespace broker::adapters::secondary

