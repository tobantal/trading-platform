#pragma once

#include "ports/input/IPortfolioService.hpp"
#include "ports/output/IBrokerGateway.hpp"
#include "ports/output/IAccountRepository.hpp"
#include <memory>

namespace trading::application {

/**
 * @brief Сервис управления портфелем
 * 
 * Реализует IPortfolioService для получения информации
 * о портфеле: позиции, P&L, доступные средства.
 */
class PortfolioService : public ports::input::IPortfolioService {
public:
    PortfolioService(
        std::shared_ptr<ports::output::IBrokerGateway> broker,
        std::shared_ptr<ports::output::IAccountRepository> accountRepository
    ) : broker_(std::move(broker))
      , accountRepository_(std::move(accountRepository))
    {}

    /**
     * @brief Получить портфель счёта
     */
    domain::Portfolio getPortfolio(const std::string& accountId) override {
        // В MVP используем единый FakeTinkoffAdapter для всех счетов
        // В реальности здесь был бы выбор адаптера по accountId
        return broker_->getPortfolio();
    }

    /**
     * @brief Получить позицию по FIGI
     */
    std::optional<domain::Position> getPosition(
        const std::string& accountId,
        const std::string& figi
    ) override {
        auto positions = broker_->getPositions();
        for (const auto& pos : positions) {
            if (pos.figi == figi) {
                return pos;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Получить общую стоимость портфеля
     */
    domain::Money getTotalValue(const std::string& accountId) override {
        auto portfolio = broker_->getPortfolio();
        return portfolio.totalValue;
    }

    /**
     * @brief Получить доступные средства
     */
    domain::Money getAvailableCash(const std::string& accountId) override {
        return broker_->getCash();
    }

private:
    std::shared_ptr<ports::output::IBrokerGateway> broker_;
    std::shared_ptr<ports::output::IAccountRepository> accountRepository_;
};

} // namespace trading::application
