#pragma once

#include "ports/input/IPortfolioService.hpp"
#include "ports/output/IBrokerGateway.hpp"
#include <memory>
#include <iostream>

namespace trading::application {

/**
 * @brief Сервис портфеля
 * 
 * Получает данные через HTTP от broker-service.
 * Не кэшируется - нужны актуальные данные.
 */
class PortfolioService : public ports::input::IPortfolioService {
public:
    explicit PortfolioService(
        std::shared_ptr<ports::output::IBrokerGateway> broker
    ) : broker_(std::move(broker))
    {
        std::cout << "[PortfolioService] Created" << std::endl;
    }

    /**
     * @brief Получить портфель аккаунта
     */
    domain::Portfolio getPortfolio(const std::string& accountId) override {
        return broker_->getPortfolio(accountId);
    }

    /**
     * @brief Получить доступные денежные средства
     */
    domain::Money getAvailableCash(const std::string& accountId) override {
        auto portfolio = broker_->getPortfolio(accountId);
        return portfolio.cash;
    }

    /**
     * @brief Получить позиции портфеля
     */
    std::vector<domain::Position> getPositions(const std::string& accountId) override {
        auto portfolio = broker_->getPortfolio(accountId);
        return portfolio.positions;
    }

private:
    std::shared_ptr<ports::output::IBrokerGateway> broker_;
};

} // namespace trading::application
