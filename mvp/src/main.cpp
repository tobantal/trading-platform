#include <iostream>
#include <memory>

// Domain
#include "domain/Money.hpp"
#include "domain/Timestamp.hpp"
#include "domain/Account.hpp"
#include "domain/Quote.hpp"
#include "domain/Position.hpp"
#include "domain/Order.hpp"
#include "domain/Strategy.hpp"
#include "domain/Signal.hpp"
#include "domain/User.hpp"
#include "domain/events/OrderCreatedEvent.hpp"


// Input Ports
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include "ports/input/IMarketService.hpp"
#include "ports/input/IOrderService.hpp"
#include "ports/input/IPortfolioService.hpp"
#include "ports/input/IStrategyService.hpp"

// Output Ports
#include "ports/output/IBrokerGateway.hpp"
#include "ports/output/IUserRepository.hpp"
#include "ports/output/IAccountRepository.hpp"
#include "ports/output/IOrderRepository.hpp"
#include "ports/output/IStrategyRepository.hpp"
#include "ports/output/IJwtProvider.hpp"
#include "ports/output/ICachePort.hpp"
#include "ports/output/IEventBus.hpp"

// Common lib
#include <ThreadSafeMap.hpp>

int main(int argc, char* argv[]) {
    std::cout << "╔════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║        Trading Platform MVP - Structure Check      ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    // ============================================
    // Проверка Domain Entities
    // ============================================
    std::cout << "✓ Domain Entities:" << std::endl;
    
    trading::domain::Money price = trading::domain::Money::fromDouble(265.50, "RUB");
    std::cout << "  - Money: " << price.toDouble() << " " << price.currency << std::endl;

    trading::domain::Timestamp now = trading::domain::Timestamp::now();
    std::cout << "  - Timestamp: " << now.toString() << std::endl;

    trading::domain::User user("user-123", "trader1");
    std::cout << "  - User: " << user.username << std::endl;

    trading::domain::Account account("acc-123", user.id, "Sandbox", 
        trading::domain::AccountType::SANDBOX, "token-xxx", true);
    std::cout << "  - Account: " << account.name << " (" 
              << trading::domain::toString(account.type) << ")" << std::endl;

    trading::domain::Quote quote("BBG004730N88", "SBER", price, price, price);
    std::cout << "  - Quote: " << quote.ticker << " @ " 
              << quote.lastPrice.toDouble() << std::endl;

    trading::domain::Position position(quote.figi, quote.ticker, 100, price, price);
    std::cout << "  - Position: " << position.ticker << " x " 
              << position.quantity << " lots" << std::endl;

    trading::domain::Order order("ord-123", account.id, quote.figi,
        trading::domain::OrderDirection::BUY, trading::domain::OrderType::LIMIT, 10, price);
    std::cout << "  - Order: " << trading::domain::toString(order.direction) << " " 
              << order.quantity << " @ " << order.price.toDouble() << std::endl;

    trading::domain::Strategy strategy("str-123", account.id, "SBER SMA",
        trading::domain::StrategyType::SMA_CROSSOVER, quote.figi, R"({"shortPeriod":10})");
    std::cout << "  - Strategy: " << strategy.name << " (" 
              << trading::domain::toString(strategy.status) << ")" << std::endl;

    auto signal = trading::domain::Signal::buy("sig-123", strategy.id, quote.figi, price, 
        "SMA10 crossed above SMA30");
    std::cout << "  - Signal: " << trading::domain::toString(signal.type) 
              << " - " << signal.reason << std::endl;

    std::cout << std::endl;

    // ============================================
    // Проверка ThreadSafeMap из common-lib
    // ============================================
    std::cout << "✓ Common Library:" << std::endl;
    
    ThreadSafeMap<std::string, trading::domain::Quote> quoteCache;
    quoteCache.insert(quote.figi, std::make_shared<trading::domain::Quote>(quote));
    
    auto cached = quoteCache.find(quote.figi);
    if (cached) {
        std::cout << "  - ThreadSafeMap: cached " << cached->ticker << std::endl;
    }
    
    std::cout << "  - contains(SBER): " << (quoteCache.contains(quote.figi) ? "yes" : "no") << std::endl;
    std::cout << "  - contains(XXX): " << (quoteCache.contains("XXX") ? "yes" : "no") << std::endl;

    std::cout << std::endl;

    // ============================================
    // Проверка Events
    // ============================================
    std::cout << "✓ Domain Events:" << std::endl;

    trading::domain::OrderCreatedEvent orderEvent;
    orderEvent.eventId = "evt-123";
    orderEvent.orderId = order.id;
    orderEvent.accountId = account.id;
    orderEvent.figi = order.figi;
    orderEvent.direction = order.direction;
    orderEvent.orderType = order.type;
    orderEvent.quantity = order.quantity;
    orderEvent.price = order.price;
    
    std::cout << "  - OrderCreatedEvent JSON (first 80 chars): " 
              << orderEvent.toJson().substr(0, 80) << "..." << std::endl;

    std::cout << std::endl;
    std::cout << "════════════════════════════════════════════════════" << std::endl;
    std::cout << "Structure check completed successfully!" << std::endl;
    std::cout << "Next step: Implement adapters (to be continued...)" << std::endl;
    std::cout << "════════════════════════════════════════════════════" << std::endl;

    return 0;
}