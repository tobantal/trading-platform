#pragma once

#include "domain/Portfolio.hpp"
#include "domain/Position.hpp"
#include "domain/Money.hpp"
#include <vector>
#include <string>

namespace trading::ports::input {

/**
 * @brief Интерфейс сервиса портфеля
 */
class IPortfolioService {
public:
    virtual ~IPortfolioService() = default;

    /**
     * @brief Получить портфель аккаунта
     */
    virtual domain::Portfolio getPortfolio(const std::string& accountId) = 0;

    /**
     * @brief Получить доступные денежные средства
     */
    virtual domain::Money getAvailableCash(const std::string& accountId) = 0;

    /**
     * @brief Получить позиции портфеля
     */
    virtual std::vector<domain::Position> getPositions(const std::string& accountId) = 0;
};

} // namespace trading::ports::input
