#pragma once

#include "enums/StrategyType.hpp"
#include <string>
#include <cstdint>

namespace trading::domain {


/**
 * @brief Запрос на создание стратегии
 */
struct StrategyRequest {
    std::string accountId;      ///< ID счёта
    std::string name;           ///< Название стратегии
    StrategyType type;          ///< Тип стратегии
    std::string figi;           ///< FIGI инструмента
    std::string config;         ///< JSON конфигурации
};

} // namespace trading::domain