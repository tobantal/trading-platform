#pragma once

#include <chrono>
#include <string>
#include <cstdint>

namespace trading::adapters::secondary {

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
 * @brief Конфигурация рыночного сценария для тестирования
 * 
 * Позволяет настраивать:
 * - Ценовые параметры (базовая цена, спред, волатильность)
 * - Ликвидность и проскальзывание
 * - Поведение исполнения ордеров
 * - Вероятность отклонения
 * 
 * @example
 * ```cpp
 * MarketScenario scenario;
 * scenario.basePrice = 280.0;
 * scenario.bidAskSpread = 0.001;  // 0.1%
 * scenario.fillBehavior = OrderFillBehavior::REALISTIC;
 * ```
 */
struct MarketScenario {
    // ================================================================
    // ЦЕНОВЫЕ ПАРАМЕТРЫ
    // ================================================================
    
    double basePrice = 100.0;           ///< Базовая цена инструмента
    double bidAskSpread = 0.001;        ///< Спред bid/ask (в долях, 0.001 = 0.1%)
    double volatility = 0.002;          ///< Волатильность за тик (в долях)
    
    // ================================================================
    // ЛИКВИДНОСТЬ
    // ================================================================
    
    int64_t availableLiquidity = 10000; ///< Доступный объём (в лотах)
    double slippagePercent = 0.001;     ///< Проскальзывание при большом объёме (в долях)
    
    // ================================================================
    // ПОВЕДЕНИЕ ОРДЕРОВ
    // ================================================================
    
    OrderFillBehavior fillBehavior = OrderFillBehavior::REALISTIC;
    std::chrono::milliseconds fillDelay{0};  ///< Задержка исполнения (для DELAYED)
    double partialFillRatio = 1.0;           ///< Доля исполнения (0.0-1.0, для PARTIAL)
    
    // ================================================================
    // REJECTION
    // ================================================================
    
    double rejectProbability = 0.0;     ///< Вероятность отклонения (0.0-1.0)
    std::string rejectReason;           ///< Причина отклонения (если не пустая)
    
    // ================================================================
    // FACTORY METHODS
    // ================================================================
    
    /**
     * @brief Создать сценарий с мгновенным исполнением (для простых тестов)
     */
    static MarketScenario immediate(double price = 100.0) {
        MarketScenario s;
        s.basePrice = price;
        s.fillBehavior = OrderFillBehavior::IMMEDIATE;
        return s;
    }
    
    /**
     * @brief Создать реалистичный сценарий
     */
    static MarketScenario realistic(double price, double spread = 0.001, double volatility = 0.002) {
        MarketScenario s;
        s.basePrice = price;
        s.bidAskSpread = spread;
        s.volatility = volatility;
        s.fillBehavior = OrderFillBehavior::REALISTIC;
        return s;
    }
    
    /**
     * @brief Создать сценарий с низкой ликвидностью (высокий slippage)
     */
    static MarketScenario lowLiquidity(double price, int64_t liquidity = 100) {
        MarketScenario s;
        s.basePrice = price;
        s.availableLiquidity = liquidity;
        s.slippagePercent = 0.01;  // 1%
        s.fillBehavior = OrderFillBehavior::REALISTIC;
        return s;
    }
    
    /**
     * @brief Создать сценарий с частичным исполнением
     */
    static MarketScenario partialFill(double price, double ratio = 0.5) {
        MarketScenario s;
        s.basePrice = price;
        s.fillBehavior = OrderFillBehavior::PARTIAL;
        s.partialFillRatio = ratio;
        return s;
    }
    
    /**
     * @brief Создать сценарий с гарантированным отклонением
     */
    static MarketScenario alwaysReject(const std::string& reason = "Test rejection") {
        MarketScenario s;
        s.fillBehavior = OrderFillBehavior::ALWAYS_REJECT;
        s.rejectProbability = 1.0;
        s.rejectReason = reason;
        return s;
    }
    
    /**
     * @brief Создать высоковолатильный сценарий
     */
    static MarketScenario highVolatility(double price, double vol = 0.05) {
        MarketScenario s;
        s.basePrice = price;
        s.volatility = vol;  // 5% за тик
        s.fillBehavior = OrderFillBehavior::REALISTIC;
        return s;
    }
};

} // namespace trading::adapters::secondary
