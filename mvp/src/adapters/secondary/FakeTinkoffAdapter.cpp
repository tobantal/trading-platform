// src/adapters/secondary/FakeTinkoffAdapter.cpp
#include "adapters/secondary/broker/FakeTinkoffAdapter.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <iostream>

namespace trading::adapters::secondary {

FakeTinkoffAdapter::FakeTinkoffAdapter() 
    : rng_(std::random_device{}()) 
{
    initInstruments();
    initTestAccounts();
}

void FakeTinkoffAdapter::initInstruments() {
    instruments_["BBG004730N88"] = domain::Instrument("BBG004730N88", "SBER", "Сбербанк", "RUB", 10);
    basePrices_["BBG004730N88"] = 265.0;

    instruments_["BBG004731032"] = domain::Instrument("BBG004731032", "LKOH", "ЛУКОЙЛ", "RUB", 1);
    basePrices_["BBG004731032"] = 7200.0;

    instruments_["BBG004730RP0"] = domain::Instrument("BBG004730RP0", "GAZP", "Газпром", "RUB", 10);
    basePrices_["BBG004730RP0"] = 128.0;

    instruments_["BBG004730ZJ9"] = domain::Instrument("BBG004730ZJ9", "VTBR", "ВТБ", "RUB", 10000);
    basePrices_["BBG004730ZJ9"] = 0.02;

    instruments_["BBG006L8G4H1"] = domain::Instrument("BBG006L8G4H1", "YNDX", "Яндекс", "RUB", 1);
    basePrices_["BBG006L8G4H1"] = 3500.0;
}

// ============================================================================
// Инициализация тестовых аккаунтов
// ============================================================================
// ID аккаунтов ДОЛЖНЫ совпадать с InMemoryAccountRepository!
// ============================================================================
void FakeTinkoffAdapter::initTestAccounts() {
    // ----------------------------------------------------------------
    // trader1 (user-001): 2 аккаунта — sandbox + production
    // ----------------------------------------------------------------
    registerAccount("acc-001-sandbox", "fake-token-001-sandbox");
    setCash("acc-001-sandbox", domain::Money::fromDouble(1000000.0, "RUB"));
    
    registerAccount("acc-001-prod", "fake-token-001-prod");
    setCash("acc-001-prod", domain::Money::fromDouble(500000.0, "RUB"));

    // ----------------------------------------------------------------
    // trader2 (user-002): 1 аккаунт — только sandbox
    // ----------------------------------------------------------------
    registerAccount("acc-002-sandbox", "fake-token-002-sandbox");
    setCash("acc-002-sandbox", domain::Money::fromDouble(100000.0, "RUB"));

    // ----------------------------------------------------------------
    // newbie (user-003): 0 аккаунтов — для теста "добавить аккаунт"
    // ----------------------------------------------------------------
    // (ничего не регистрируем)

    // ----------------------------------------------------------------
    // admin (user-004): 1 аккаунт — sandbox
    // ----------------------------------------------------------------
    registerAccount("acc-004-sandbox", "fake-token-004-sandbox");
    setCash("acc-004-sandbox", domain::Money::fromDouble(10000000.0, "RUB"));

    std::cout << "[FakeTinkoffAdapter] Initialized 4 test accounts:" << std::endl;
    std::cout << "  - acc-001-sandbox (trader1): 1,000,000 RUB" << std::endl;
    std::cout << "  - acc-001-prod    (trader1): 500,000 RUB" << std::endl;
    std::cout << "  - acc-002-sandbox (trader2): 100,000 RUB" << std::endl;
    std::cout << "  - acc-004-sandbox (admin):   10,000,000 RUB" << std::endl;
}

domain::Money FakeTinkoffAdapter::generatePrice(const std::string& figi) {
    auto it = basePrices_.find(figi);
    if (it == basePrices_.end()) {
        return domain::Money();
    }

    std::lock_guard<std::mutex> lock(rngMutex_);
    std::uniform_real_distribution<double> dist(-0.05, 0.05);
    double price = it->second * (1.0 + dist(rng_));
    
    return domain::Money::fromDouble(price, "RUB");
}

std::string FakeTinkoffAdapter::generateUuid() {
    std::lock_guard<std::mutex> lock(rngMutex_);
    std::uniform_int_distribution<uint64_t> dist;
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << (dist(rng_) & 0xFFFFFFFF) << "-";
    ss << std::setw(4) << (dist(rng_) & 0xFFFF) << "-";
    ss << std::setw(4) << ((dist(rng_) & 0x0FFF) | 0x4000) << "-";
    ss << std::setw(4) << ((dist(rng_) & 0x3FFF) | 0x8000) << "-";
    ss << std::setw(12) << (dist(rng_) & 0xFFFFFFFFFFFF);
    
    return ss.str();
}

std::shared_ptr<FakeTinkoffAdapter::AccountData> FakeTinkoffAdapter::getOrCreateAccount(const std::string& accountId) {
    std::lock_guard<std::mutex> lock(accountsMutex_);
    
    auto it = accounts_.find(accountId);
    if (it == accounts_.end()) {
        auto account = std::make_shared<AccountData>("default-token");
        accounts_[accountId] = account;
        return account;
    }
    
    return it->second;
}

std::shared_ptr<FakeTinkoffAdapter::AccountData> FakeTinkoffAdapter::getAccount(const std::string& accountId) {
    std::lock_guard<std::mutex> lock(accountsMutex_);
    auto it = accounts_.find(accountId);
    return it != accounts_.end() ? it->second : nullptr;
}

void FakeTinkoffAdapter::registerAccount(const std::string& accountId, const std::string& accessToken) {
    std::lock_guard<std::mutex> lock(accountsMutex_);
    
    auto account = std::make_shared<AccountData>(accessToken);
    accounts_[accountId] = account;
}

void FakeTinkoffAdapter::unregisterAccount(const std::string& accountId) {
    std::lock_guard<std::mutex> lock(accountsMutex_);
    accounts_.erase(accountId);
}

std::optional<domain::Quote> FakeTinkoffAdapter::getQuote(const std::string& figi) {
    auto it = instruments_.find(figi);
    if (it == instruments_.end()) {
        return std::nullopt;
    }

    domain::Money lastPrice = generatePrice(figi);
    domain::Money spread = domain::Money::fromDouble(lastPrice.toDouble() * 0.001, "RUB");
    
    return domain::Quote(figi, it->second.ticker, lastPrice, lastPrice - spread, lastPrice + spread);
}

std::vector<domain::Quote> FakeTinkoffAdapter::getQuotes(const std::vector<std::string>& figis) {
    std::vector<domain::Quote> result;
    for (const auto& figi : figis) {
        if (auto quote = getQuote(figi)) {
            result.push_back(*quote);
        }
    }
    return result;
}

std::vector<domain::Instrument> FakeTinkoffAdapter::searchInstruments(const std::string& query) {
    std::vector<domain::Instrument> result;
    std::string q = query;
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
    
    for (const auto& [figi, instr] : instruments_) {
        std::string ticker = instr.ticker;
        std::transform(ticker.begin(), ticker.end(), ticker.begin(), ::tolower);
        std::string name = instr.name;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        
        if (ticker.find(q) != std::string::npos || name.find(q) != std::string::npos) {
            result.push_back(instr);
        }
    }
    return result;
}

std::optional<domain::Instrument> FakeTinkoffAdapter::getInstrumentByFigi(const std::string& figi) {
    auto it = instruments_.find(figi);
    return it != instruments_.end() ? std::optional(it->second) : std::nullopt;
}

std::vector<domain::Instrument> FakeTinkoffAdapter::getAllInstruments() {
    std::vector<domain::Instrument> result;
    for (const auto& [figi, instr] : instruments_) {
        result.push_back(instr);
    }
    return result;
}

void FakeTinkoffAdapter::updatePortfolioPrices(domain::Portfolio& portfolio) {
    for (auto& pos : portfolio.positions) {
        if (auto quote = getQuote(pos.figi)) {
            pos.updateCurrentPrice(quote->lastPrice);
        }
    }
    portfolio.recalculateTotalValue();
}

domain::Portfolio FakeTinkoffAdapter::getPortfolio(const std::string& accountId) {
    auto account = getAccount(accountId);
    if (!account) {
        throw std::runtime_error("Account not found: " + accountId);
    }
    
    domain::Portfolio portfolio;
    portfolio.accountId = accountId;
    portfolio.cash = account->cash;
    
    auto allPositions = account->positions.getAll();
    for (const auto& pos : allPositions) {
        portfolio.positions.push_back(*pos);
    }
    
    updatePortfolioPrices(portfolio);
    return portfolio;
}

void FakeTinkoffAdapter::executeOrder(AccountData& account, const domain::Order& order, const domain::Instrument& instrument) {
    domain::Money totalCost = order.price * (order.quantity * instrument.lot);
    
    if (order.direction == domain::OrderDirection::BUY) {
        account.cash = account.cash - totalCost;
        
        auto existing = account.positions.find(order.figi);
        if (existing) {
            int64_t newQty = existing->quantity + order.quantity;
            double avgPrice = (existing->averagePrice.toDouble() * existing->quantity +
                               order.price.toDouble() * order.quantity) / newQty;
            
            auto updated = std::make_shared<domain::Position>(*existing);
            updated->quantity = newQty;
            updated->averagePrice = domain::Money::fromDouble(avgPrice, "RUB");
            updated->updateCurrentPrice(order.price);
            
            account.positions.insert(order.figi, updated);
        } else {
            auto position = std::make_shared<domain::Position>(
                order.figi, instrument.ticker, order.quantity, 
                order.price, order.price);
            account.positions.insert(order.figi, position);
        }
    } else {
        account.cash = account.cash + totalCost;
        
        auto existing = account.positions.find(order.figi);
        if (existing) {
            int64_t newQty = existing->quantity - order.quantity;
            if (newQty <= 0) {
                account.positions.remove(order.figi);
            } else {
                auto updated = std::make_shared<domain::Position>(*existing);
                updated->quantity = newQty;
                updated->updateCurrentPrice(order.price);
                account.positions.insert(order.figi, updated);
            }
        }
    }
}

domain::OrderResult FakeTinkoffAdapter::placeOrder(const std::string& accountId, const domain::OrderRequest& request) {
    auto account = getAccount(accountId);
    if (!account) {
        domain::OrderResult result;
        result.status = domain::OrderStatus::REJECTED;
        result.message = "Account not found: " + accountId;
        return result;
    }
    
    auto instrument = getInstrumentByFigi(request.figi);
    if (!instrument) {
        domain::OrderResult result;
        result.status = domain::OrderStatus::REJECTED;
        result.message = "Instrument not found: " + request.figi;
        return result;
    }
    
    domain::Money price = (request.type == domain::OrderType::MARKET)
        ? generatePrice(request.figi)
        : request.price;
    
    domain::Money totalCost = price * (request.quantity * instrument->lot);
    
    // Проверка средств для покупки
    if (request.direction == domain::OrderDirection::BUY) {
        if (account->cash.toDouble() < totalCost.toDouble()) {
            domain::OrderResult result;
            result.status = domain::OrderStatus::REJECTED;
            result.message = "Insufficient funds";
            return result;
        }
    } else {
        // Проверка позиции для продажи
        auto pos = account->positions.find(request.figi);
        if (!pos || pos->quantity < request.quantity) {
            domain::OrderResult result;
            result.status = domain::OrderStatus::REJECTED;
            result.message = "Insufficient position";
            return result;
        }
    }
    
    // Для лимитных ордеров - ставим в pending
    if (request.type == domain::OrderType::LIMIT) {
        std::string orderId = generateUuid();
        
        domain::Order order;
        order.id = orderId;
        order.accountId = accountId;
        order.figi = request.figi;
        order.direction = request.direction;
        order.type = request.type;
        order.quantity = request.quantity;
        order.price = request.price;
        order.status = domain::OrderStatus::PENDING;
        order.createdAt = domain::Timestamp::now();
        order.updatedAt = domain::Timestamp::now();
        
        account->orders.insert(orderId, std::make_shared<domain::Order>(order));
        
        domain::OrderResult result;
        result.orderId = orderId;
        result.status = domain::OrderStatus::PENDING;
        result.message = "Limit order placed";
        result.timestamp = domain::Timestamp::now();
        return result;
    }
    
    // Market ордер - исполняем сразу
    std::string orderId = generateUuid();
    
    domain::Order order;
    order.id = orderId;
    order.accountId = accountId;
    order.figi = request.figi;
    order.direction = request.direction;
    order.type = request.type;
    order.quantity = request.quantity;
    order.price = price;
    order.status = domain::OrderStatus::FILLED;
    order.createdAt = domain::Timestamp::now();
    order.updatedAt = domain::Timestamp::now();
    
    executeOrder(*account, order, *instrument);
    account->orders.insert(orderId, std::make_shared<domain::Order>(order));
    
    domain::OrderResult result;
    result.orderId = orderId;
    result.status = domain::OrderStatus::FILLED;
    result.executedPrice = price;
    result.message = "Order filled";
    result.timestamp = domain::Timestamp::now();
    return result;
}

bool FakeTinkoffAdapter::cancelOrder(const std::string& accountId, const std::string& orderId) {
    auto account = getAccount(accountId);
    if (!account) {
        return false;
    }
    
    auto order = account->orders.find(orderId);
    if (!order || order->status != domain::OrderStatus::PENDING) {
        return false;
    }
    
    auto updated = std::make_shared<domain::Order>(*order);
    updated->status = domain::OrderStatus::CANCELLED;
    updated->updatedAt = domain::Timestamp::now();
    account->orders.insert(orderId, updated);
    
    return true;
}

std::optional<domain::Order> FakeTinkoffAdapter::getOrderStatus(const std::string& accountId, const std::string& orderId) {
    auto account = getAccount(accountId);
    if (!account) {
        return std::nullopt;
    }
    
    auto order = account->orders.find(orderId);
    return order ? std::optional(*order) : std::nullopt;
}

std::vector<domain::Order> FakeTinkoffAdapter::getOrders(const std::string& accountId) {
    auto account = getAccount(accountId);
    if (!account) {
        return {};
    }
    
    std::vector<domain::Order> result;
    auto allOrders = account->orders.getAll();
    for (const auto& order : allOrders) {
        result.push_back(*order);
    }
    return result;
}

std::vector<domain::Order> FakeTinkoffAdapter::getOrderHistory(
    const std::string& accountId,
    const std::optional<std::chrono::system_clock::time_point>& from,
    const std::optional<std::chrono::system_clock::time_point>& to) 
{
    // Для простоты возвращаем все ордера (фильтрация по времени не реализована)
    return getOrders(accountId);
}

void FakeTinkoffAdapter::reset() {
    {
        std::lock_guard<std::mutex> lock(accountsMutex_);
        accounts_.clear();
    }
     initInstruments();
     initTestAccounts();
}

void FakeTinkoffAdapter::setCash(const std::string& accountId, const domain::Money& cash) {
    auto account = getAccount(accountId);
    if (account) {
        account->cash = cash;
    }
}

void FakeTinkoffAdapter::setPositions(const std::string& accountId, const std::vector<domain::Position>& positions) {
    auto account = getAccount(accountId);
    if (account) {
        account->positions.clear();
        for (const auto& pos : positions) {
            account->positions.insert(pos.figi, std::make_shared<domain::Position>(pos));
        }
    }
}

} // namespace trading::adapters::secondary
