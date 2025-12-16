#pragma once

#include "domain/Portfolio.hpp"
#include "domain/Position.hpp"
#include <string>
#include <optional>

namespace trading::ports::input {

/**
 * @brief Интерфейс сервиса работы с портфелем
 * 
 * Input Port для получения информации о портфеле и позициях.
 */
class IPortfolioService {
public:
    virtual ~IPortfolioService() = default;

    /**
     * @brief Получить полный портфель счёта
     * 
     * @param accountId ID счёта
     * @return Portfolio с позициями и кэшем
     */
    virtual domain::Portfolio getPortfolio(const std::string& accountId) = 0;

    /**
     * @brief Получить позицию по конкретному инструменту
     * 
     * @param accountId ID счёта
     * @param figi FIGI инструмента
     * @return Position или nullopt если позиции нет
     */
    virtual std::optional<domain::Position> getPosition(
        const std::string& accountId,
        const std::string& figi
    ) = 0;

    /**
     * @brief Получить общую стоимость портфеля
     * 
     * @param accountId ID счёта
     * @return Общая стоимость (позиции + кэш)
     */
    virtual domain::Money getTotalValue(const std::string& accountId) = 0;

    /**
     * @brief Получить свободные денежные средства
     * 
     * @param accountId ID счёта
     * @return Сумма свободного кэша
     */
    virtual domain::Money getAvailableCash(const std::string& accountId) = 0;
};

} // namespace trading::ports::input