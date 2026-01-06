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
 * @brief Симулятор рыночных цен
 * 
 * Генерирует реалистичные bid/ask цены с:
 * - Random walk (случайное блуждание)
 * - Настраиваемой волатильностью
 * - Bid-ask спредом
 * 
 * Использует упрощённую модель geometric Brownian motion:
 * dS = σ * S * dW
 * где σ - волатильность, dW - винеровский процесс
 * 
 * @example
 * ```cpp
 * PriceSimulator sim;
 * sim.initInstrument("SBER", 280.0, 0.001, 0.002);
 * 
 * auto quote = sim.getQuote("SBER");
 * // quote.bid ≈ 279.86
 * // quote.ask ≈ 280.14
 * 
 * sim.tick("SBER");  // Цена изменится случайно
 * ```
 * 
 * Thread-safe: да (внутренняя синхронизация)
 */
class PriceSimulator {
public:
    /**
     * @brief Котировка инструмента
     */
    struct Quote {
        double bid = 0.0;           ///< Лучшая цена покупки
        double ask = 0.0;           ///< Лучшая цена продажи
        double last = 0.0;          ///< Последняя сделка
        int64_t volume = 0;         ///< Объём за день
        std::chrono::system_clock::time_point timestamp;
        
        /**
         * @brief Средняя цена (mid)
         */
        double mid() const {
            return (bid + ask) / 2.0;
        }
        
        /**
         * @brief Спред в абсолютных единицах
         */
        double spreadAbs() const {
            return ask - bid;
        }
        
        /**
         * @brief Спред в процентах
         */
        double spreadPercent() const {
            if (mid() == 0.0) return 0.0;
            return spreadAbs() / mid() * 100.0;
        }
    };
    
    /**
     * @brief Конструктор
     * @param seed Seed для генератора случайных чисел (0 = random_device)
     */
    explicit PriceSimulator(unsigned int seed = 0)
        : rng_(seed == 0 ? std::random_device{}() : seed)
    {}
    
    /**
     * @brief Инициализировать инструмент
     * 
     * @param figi Идентификатор инструмента
     * @param basePrice Начальная цена
     * @param spread Спред bid/ask в долях (0.001 = 0.1%)
     * @param volatility Волатильность за тик в долях (0.002 = 0.2%)
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
     * @brief Проверить, инициализирован ли инструмент
     */
    bool hasInstrument(const std::string& figi) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return instruments_.find(figi) != instruments_.end();
    }
    
    /**
     * @brief Получить текущую котировку
     * 
     * @param figi Идентификатор инструмента
     * @return Котировка или nullopt если инструмент не найден
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
     * @brief Симилировать один тик (движение цены)
     * 
     * Использует geometric random walk:
     * P(t+1) = P(t) * (1 + σ * Z)
     * где Z ~ N(0, 1)
     * 
     * @param figi Идентификатор инструмента
     * @return Новая цена или 0.0 если инструмент не найден
     */
    double tick(const std::string& figi) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = instruments_.find(figi);
        if (it == instruments_.end()) {
            return 0.0;
        }
        
        auto& state = it->second;
        
        // Random walk с нормальным распределением
        std::normal_distribution<double> dist(0.0, state.volatility);
        double change = dist(rng_);
        
        state.currentPrice *= (1.0 + change);
        state.currentPrice = std::max(0.01, state.currentPrice);  // Минимум 1 копейка
        state.lastUpdate = std::chrono::system_clock::now();
        
        return state.currentPrice;
    }
    
    /**
     * @brief Симилировать N тиков
     * 
     * @param figi Идентификатор инструмента
     * @param ticks Количество тиков
     * @return Финальная цена
     */
    double simulate(const std::string& figi, int ticks) {
        double price = 0.0;
        for (int i = 0; i < ticks; ++i) {
            price = tick(figi);
        }
        return price;
    }
    
    /**
     * @brief Установить конкретную цену (для детерминированных тестов)
     * 
     * @param figi Идентификатор инструмента
     * @param price Новая цена
     * @return true если инструмент найден
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
     * @brief Сдвинуть цену на delta
     * 
     * @param figi Идентификатор инструмента
     * @param delta Изменение цены (может быть отрицательным)
     * @return Новая цена или 0.0 если инструмент не найден
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
     * @brief Изменить цену на процент
     * 
     * @param figi Идентификатор инструмента
     * @param percent Изменение в процентах (-5.0 = -5%)
     * @return Новая цена или 0.0 если инструмент не найден
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
     * @brief Получить текущую цену (без bid/ask)
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
     * @brief Изменить волатильность инструмента
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
     * @brief Изменить спред инструмента
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
     * @brief Удалить инструмент
     */
    bool removeInstrument(const std::string& figi) {
        std::lock_guard<std::mutex> lock(mutex_);
        return instruments_.erase(figi) > 0;
    }
    
    /**
     * @brief Очистить все инструменты
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        instruments_.clear();
    }
    
    /**
     * @brief Получить количество инструментов
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