/**
 * @file PortfolioServiceTest.cpp
 * @brief Unit tests for PortfolioService
 */

#include <gtest/gtest.h>
#include "application/PortfolioService.hpp"
#include "../mocks/MockBrokerGateway.hpp"

using namespace trading;
using namespace trading::application;
using namespace trading::tests;

class PortfolioServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockBroker_ = std::make_shared<MockBrokerGateway>();
        portfolioService_ = std::make_shared<PortfolioService>(mockBroker_);

        setupTestData();
    }

    void setupTestData() {
        domain::Portfolio portfolio;
        portfolio.cash = domain::Money::fromDouble(100000.0, "RUB");
        portfolio.totalValue = domain::Money::fromDouble(150000.0, "RUB");

        domain::Position pos1;
        pos1.figi = "BBG004730N88";
        pos1.ticker = "SBER";
        pos1.quantity = 100;
        pos1.averagePrice = domain::Money::fromDouble(270.0, "RUB");
        pos1.currentPrice = domain::Money::fromDouble(280.0, "RUB");
        pos1.pnl = domain::Money::fromDouble(1000.0, "RUB");
        pos1.pnlPercent = 3.7;
        portfolio.positions.push_back(pos1);

        domain::Position pos2;
        pos2.figi = "BBG004730RP0";
        pos2.ticker = "GAZP";
        pos2.quantity = 200;
        pos2.averagePrice = domain::Money::fromDouble(145.0, "RUB");
        pos2.currentPrice = domain::Money::fromDouble(150.0, "RUB");
        pos2.pnl = domain::Money::fromDouble(1000.0, "RUB");
        pos2.pnlPercent = 3.45;
        portfolio.positions.push_back(pos2);

        mockBroker_->setPortfolio("acc-001", portfolio);
    }

    std::shared_ptr<MockBrokerGateway> mockBroker_;
    std::shared_ptr<PortfolioService> portfolioService_;
};

// ============================================================================
// GET PORTFOLIO TESTS
// ============================================================================

TEST_F(PortfolioServiceTest, GetPortfolio_ExistingAccount_ReturnsPortfolio) {
    auto portfolio = portfolioService_->getPortfolio("acc-001");

    EXPECT_NEAR(portfolio.cash.toDouble(), 100000.0, 0.01);
    EXPECT_NEAR(portfolio.totalValue.toDouble(), 150000.0, 0.01);
    EXPECT_EQ(portfolio.positions.size(), 2u);
}

TEST_F(PortfolioServiceTest, GetPortfolio_UnknownAccount_ReturnsEmpty) {
    auto portfolio = portfolioService_->getPortfolio("unknown");

    EXPECT_NEAR(portfolio.cash.toDouble(), 0.0, 0.01);
    EXPECT_TRUE(portfolio.positions.empty());
}

// ============================================================================
// GET AVAILABLE CASH TESTS
// ============================================================================

TEST_F(PortfolioServiceTest, GetAvailableCash_ExistingAccount_ReturnsCash) {
    auto cash = portfolioService_->getAvailableCash("acc-001");

    EXPECT_NEAR(cash.toDouble(), 100000.0, 0.01);
    EXPECT_EQ(cash.currency, "RUB");
}

TEST_F(PortfolioServiceTest, GetAvailableCash_UnknownAccount_ReturnsZero) {
    auto cash = portfolioService_->getAvailableCash("unknown");

    EXPECT_NEAR(cash.toDouble(), 0.0, 0.01);
}

// ============================================================================
// GET POSITIONS TESTS
// ============================================================================

TEST_F(PortfolioServiceTest, GetPositions_ExistingAccount_ReturnsPositions) {
    auto positions = portfolioService_->getPositions("acc-001");

    ASSERT_EQ(positions.size(), 2u);
    
    EXPECT_EQ(positions[0].ticker, "SBER");
    EXPECT_EQ(positions[0].quantity, 100);
    EXPECT_NEAR(positions[0].pnl.toDouble(), 1000.0, 0.01);

    EXPECT_EQ(positions[1].ticker, "GAZP");
    EXPECT_EQ(positions[1].quantity, 200);
}

TEST_F(PortfolioServiceTest, GetPositions_UnknownAccount_ReturnsEmpty) {
    auto positions = portfolioService_->getPositions("unknown");

    EXPECT_TRUE(positions.empty());
}

// ============================================================================
// TOTAL PNL TESTS
// ============================================================================

TEST_F(PortfolioServiceTest, Portfolio_TotalPnl_SumsPositions) {
    auto portfolio = portfolioService_->getPortfolio("acc-001");
    auto totalPnl = portfolio.totalPnl();

    // Сумма PnL по позициям: 1000 + 1000 = 2000
    EXPECT_NEAR(totalPnl.toDouble(), 2000.0, 0.01);
}
