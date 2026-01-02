// include/settings/BrokerSettings.hpp
#pragma once

#include <string>
#include <cstdlib>
#include <stdexcept>

namespace broker::settings {

/**
 * @brief Настройки поведения fake-брокера
 * 
 * Читает параметры из переменных окружения (K8s ENV).
 * Позволяет настроить различные сценарии исполнения ордеров
 * для тестирования и демонстрации.
 * 
 * Переменные окружения:
 * - BROKER_FILL_BEHAVIOR: IMMEDIATE, REALISTIC, PARTIAL, ALWAYS_REJECT
 * - BROKER_SLIPPAGE: проскальзывание (0.001 = 0.1%)
 * - BROKER_PARTIAL_RATIO: доля частичного исполнения (0.5 = 50%)
 * - BROKER_TICK_INTERVAL_MS: интервал тиков в мс
 * - BROKER_ENABLE_TICKER: включить фоновую симуляцию цен
 * 
 * @example K8s ConfigMap:
 * ```yaml
 * apiVersion: v1
 * kind: ConfigMap
 * metadata:
 *   name: broker-config
 * data:
 *   BROKER_FILL_BEHAVIOR: "REALISTIC"
 *   BROKER_SLIPPAGE: "0.001"
 *   BROKER_PARTIAL_RATIO: "0.5"
 *   BROKER_TICK_INTERVAL_MS: "100"
 *   BROKER_ENABLE_TICKER: "true"
 * ```
 */
class BrokerSettings {
public:
    /**
     * @brief Конструктор - читает настройки из ENV
     */
    BrokerSettings() {
        fillBehavior_ = getEnvOrDefault("BROKER_FILL_BEHAVIOR", "REALISTIC");
        slippage_ = std::stod(getEnvOrDefault("BROKER_SLIPPAGE", "0.001"));
        partialRatio_ = std::stod(getEnvOrDefault("BROKER_PARTIAL_RATIO", "0.5"));
        tickIntervalMs_ = std::stoi(getEnvOrDefault("BROKER_TICK_INTERVAL_MS", "100"));
        enableTicker_ = getEnvOrDefault("BROKER_ENABLE_TICKER", "true") == "true";
    }
    
    /**
     * @brief Получить режим исполнения ордеров
     * @return IMMEDIATE, REALISTIC, PARTIAL, ALWAYS_REJECT
     */
    std::string getFillBehavior() const { return fillBehavior_; }
    
    /**
     * @brief Получить величину проскальзывания
     * @return Проскальзывание как доля (0.001 = 0.1%)
     */
    double getSlippage() const { return slippage_; }
    
    /**
     * @brief Получить коэффициент частичного исполнения
     * @return Доля исполнения (0.5 = 50%)
     */
    double getPartialRatio() const { return partialRatio_; }
    
    /**
     * @brief Получить интервал тиков в миллисекундах
     */
    int getTickIntervalMs() const { return tickIntervalMs_; }
    
    /**
     * @brief Включён ли фоновый тикер
     */
    bool isTickerEnabled() const { return enableTicker_; }

private:
    std::string fillBehavior_;
    double slippage_;
    double partialRatio_;
    int tickIntervalMs_;
    bool enableTicker_;
    
    /**
     * @brief Получить значение ENV или вернуть default
     */
    static std::string getEnvOrDefault(const char* name, const char* defaultValue) {
        const char* value = std::getenv(name);
        return value ? std::string(value) : std::string(defaultValue);
    }
};

} // namespace broker::settings
