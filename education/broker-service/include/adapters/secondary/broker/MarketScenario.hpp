#pragma once

#include <chrono>
#include <string>
#include <cstdint>

namespace broker::adapters::secondary {

/**
 * @brief Поведение исполнения ордеров
 */
enum class OrderFillBehavior {
    IMMEDIATE,        ///< Мгновенное исполнение (текущее поведение)
    REALISTIC,        ///< Реалистичное: market=fill, limit=pending до цены
    PARTIAL,          ///< Частичное исполнение
    DELAYED,          ///< С задержкой (async)
    ALWAYS_REJECT     ///< Всегда отклонять (тест ошибок)
};

/**
 * @brief Конвертация в строку
 */
inline std::string toString(OrderFillBehavior behavior) {
    switch (behavior) {
        case OrderFillBehavior::IMMEDIATE: return "IMMEDIATE";
        case OrderFillBehavior::REALISTIC: return "REALISTIC";
        case OrderFillBehavior::PARTIAL: return "PARTIAL";
        case OrderFillBehavior::DELAYED: return "DELAYED";
        case OrderFillBehavior::ALWAYS_REJECT: return "ALWAYS_REJECT";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Парсинг из строки
 */
inline OrderFillBehavior parseOrderFillBehavior(const std::string& str) {
    if (str == "IMMEDIATE") return OrderFillBehavior::IMMEDIATE;
    if (str == "REALISTIC") return OrderFillBehavior::REALISTIC;
    if (str == "PARTIAL") return OrderFillBehavior::PARTIAL;
    if (str == "DELAYED") return OrderFillBehavior::DELAYED;
    if (str == "ALWAYS_REJECT") return OrderFillBehavior::ALWAYS_REJECT;
    return OrderFillBehavior::REALISTIC;  // По умолчанию
}

/**
 * @brief Сценарий рыночного поведения для инструмента
 * 
 * Определяет как fake-брокер будет симулировать поведение рынка:
 * - Базовая цена и волатильность
 * - Spread между bid/ask
 * - Проскальзывание при исполнении
 * - Режим исполнения ордеров
 * - Ликвидность и вероятность отклонения
 * 
 * Фабричные методы:
 * - immediate(): мгновенное исполнение без задержек
 * - realistic(): реалистичная симуляция рынка
 * - partial(): частичное исполнение с заданным коэффициентом
 * - partialFill(): алиас для partial()
 * - delayed(): асинхронное исполнение с задержкой
 * - alwaysReject(): всегда отклонять (для тестов ошибок)
 * - lowLiquidity(): низкая ликвидность с высоким проскальзыванием
 * - highVolatility(): высокая волатильность
 */
struct MarketScenario {
    // Основные параметры
    double basePrice = 100.0;           ///< Базовая цена инструмента
    double volatility = 0.01;           ///< Волатильность (дневная, σ)
    double bidAskSpread = 0.1;          ///< Spread bid-ask в процентах
    double slippage = 0.0005;           ///< Проскальзывание при исполнении (базовое)
    double partialFillRatio = 1.0;      ///< Доля исполнения (1.0 = 100%)
    OrderFillBehavior fillBehavior = OrderFillBehavior::REALISTIC;
    std::chrono::milliseconds executionDelay{0};  ///< Задержка исполнения
    
    // Дополнительные параметры для реалистичной симуляции
    int64_t availableLiquidity = 10000;   ///< Доступная ликвидность (лоты)
    double slippagePercent = 0.001;       ///< Проскальзывание в процентах
    double rejectProbability = 0.0;       ///< Вероятность отклонения [0.0 - 1.0]
    std::string rejectReason;             ///< Причина отклонения
    std::chrono::milliseconds fillDelay{0}; ///< Задержка исполнения (алиас)
    
    // ========================================================================
    // ФАБРИЧНЫЕ МЕТОДЫ
    // ========================================================================
    
    /**
     * @brief Мгновенное исполнение
     * 
     * Market и Limit ордера исполняются сразу по текущей цене.
     * Подходит для тестов без задержек.
     */
    static MarketScenario immediate(double price = 100.0) {
        MarketScenario s;
        s.basePrice = price;
        s.fillBehavior = OrderFillBehavior::IMMEDIATE;
        s.volatility = 0.0;
        s.slippage = 0.0;
        s.slippagePercent = 0.0;
        s.availableLiquidity = 1000000;
        return s;
    }
    
    /**
     * @brief Реалистичная симуляция
     * 
     * - Market ордера исполняются сразу с учётом slippage
     * - Limit ордера ждут достижения цены
     * - Цены изменяются с заданной волатильностью
     */
    static MarketScenario realistic(double price = 100.0, 
                                    double volatilityVal = 0.02, 
                                    double slippageVal = 0.001) {
        MarketScenario s;
        s.basePrice = price;
        s.fillBehavior = OrderFillBehavior::REALISTIC;
        s.volatility = volatilityVal;
        s.bidAskSpread = 0.1;
        s.slippage = slippageVal;
        s.slippagePercent = slippageVal;
        s.availableLiquidity = 10000;
        return s;
    }
    
    /**
     * @brief Частичное исполнение
     * 
     * Ордера исполняются частично (partialFillRatio от запрошенного объёма).
     * Для тестирования PARTIALLY_FILLED статуса.
     */
    static MarketScenario partial(double price = 100.0, double ratio = 0.5) {
        MarketScenario s;
        s.basePrice = price;
        s.fillBehavior = OrderFillBehavior::PARTIAL;
        s.partialFillRatio = ratio;
        s.availableLiquidity = 10000;
        return s;
    }
    
    /**
     * @brief Алиас для partial() - частичное исполнение
     */
    static MarketScenario partialFill(double price = 100.0, double ratio = 0.5) {
        return partial(price, ratio);
    }
    
    /**
     * @brief Низкая ликвидность - высокое проскальзывание
     */
    static MarketScenario lowLiquidity(double price = 100.0, int64_t liquidity = 100) {
        MarketScenario s;
        s.basePrice = price;
        s.fillBehavior = OrderFillBehavior::REALISTIC;
        s.availableLiquidity = liquidity;
        s.slippagePercent = 0.01;  // 1% проскальзывание
        s.slippage = 0.01;
        s.volatility = 0.03;
        return s;
    }
    
    /**
     * @brief Высокая волатильность
     */
    static MarketScenario highVolatility(double price = 100.0, double vol = 0.05) {
        MarketScenario s;
        s.basePrice = price;
        s.fillBehavior = OrderFillBehavior::REALISTIC;
        s.volatility = vol;
        s.slippagePercent = 0.002;
        s.availableLiquidity = 5000;
        return s;
    }
    
    /**
     * @brief С задержкой исполнения
     * 
     * Ордера исполняются асинхронно после заданной задержки.
     * Для тестирования PENDING статуса и асинхронной обработки.
     */
    static MarketScenario delayed(double price = 100.0, 
                                  std::chrono::milliseconds delay = std::chrono::milliseconds{100}) {
        MarketScenario s;
        s.basePrice = price;
        s.fillBehavior = OrderFillBehavior::DELAYED;
        s.executionDelay = delay;
        s.fillDelay = delay;
        return s;
    }
    
    /**
     * @brief Всегда отклонять
     * 
     * Все ордера отклоняются. Для тестирования обработки ошибок.
     */
    static MarketScenario alwaysReject(const std::string& reason = "Test rejection") {
        MarketScenario s;
        s.fillBehavior = OrderFillBehavior::ALWAYS_REJECT;
        s.rejectProbability = 1.0;
        s.rejectReason = reason;
        return s;
    }
    
    // ========================================================================
    // BUILDER МЕТОДЫ
    // ========================================================================
    
    MarketScenario& withVolatility(double v) { volatility = v; return *this; }
    MarketScenario& withSpread(double s) { bidAskSpread = s; return *this; }
    MarketScenario& withSlippage(double s) { slippage = s; slippagePercent = s; return *this; }
    MarketScenario& withPartialRatio(double r) { partialFillRatio = r; return *this; }
    MarketScenario& withDelay(std::chrono::milliseconds d) { executionDelay = d; fillDelay = d; return *this; }
    MarketScenario& withLiquidity(int64_t l) { availableLiquidity = l; return *this; }
    MarketScenario& withRejectProbability(double p) { rejectProbability = p; return *this; }
    MarketScenario& withRejectReason(const std::string& r) { rejectReason = r; return *this; }
};

} // namespace broker::adapters::secondary
