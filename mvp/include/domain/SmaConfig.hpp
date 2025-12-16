#pragma once

#include <string>
#include <cstdint>

namespace trading::domain {

/**
 * @brief Конфигурация SMA Crossover стратегии
 */
struct SmaConfig {
    int shortPeriod = 10;       ///< Период короткой SMA
    int longPeriod = 30;        ///< Период длинной SMA
    int64_t quantity = 1;       ///< Количество лотов на сделку

    /**
     * @brief Валидация конфигурации
     */
    bool isValid() const {
        return shortPeriod > 0 && 
               longPeriod > shortPeriod && 
               quantity > 0;
    }
};

} // namespace trading::domain