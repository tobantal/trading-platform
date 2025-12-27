#include "adapters/secondary/broker/FakeTinkoffAdapter.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace trading::adapters::secondary {

FakeTinkoffAdapter::FakeTinkoffAdapter() 
    : rng_(std::random_device{}()) 
{
    initInstruments();
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
            if (newQty > 0) {
                auto updated = std::make_shared<domain::Position>(*existing);
                updated->quantity = newQty;
                updated->updateCurrentPrice(order.price);
                account.positions.insert(order.figi, updated);
            } else {
                account.positions.remove(order.figi);
            }
        }
    }
}

domain::OrderResult FakeTinkoffAdapter::placeOrder(
    const std::string& accountId,
    const domain::OrderRequest& request
) {
    auto account = getAccount(accountId);
    if (!account) {
        domain::OrderResult result;
        result.status = domain::OrderStatus::REJECTED;
        result.message = "Account not found: " + accountId;
        return result;
    }
    
    domain::OrderResult result;
    result.timestamp = domain::Timestamp::now();
    
    auto instrument = getInstrumentByFigi(request.figi);
    if (!instrument) {
        result.status = domain::OrderStatus::REJECTED;
        result.message = "Unknown instrument: " + request.figi;
        return result;
    }
    
    auto quote = getQuote(request.figi);
    if (!quote) {
        result.status = domain::OrderStatus::REJECTED;
        result.message = "Cannot get quote";
        return result;
    }
    
    domain::Money execPrice = (request.type == domain::OrderType::MARKET) 
        ? quote->lastPrice : request.price;
    domain::Money totalCost = execPrice * (request.quantity * instrument->lot);
    
    if (request.direction == domain::OrderDirection::BUY && totalCost > account->cash) {
        result.status = domain::OrderStatus::REJECTED;
        result.message = "Insufficient funds";
        return result;
    }
    
    if (request.direction == domain::OrderDirection::SELL) {
        auto pos = account->positions.find(request.figi);
        if (!pos || pos->quantity < request.quantity) {
            result.status = domain::OrderStatus::REJECTED;
            result.message = "Insufficient position";
            return result;
        }
    }
    
    std::string orderId = generateUuid();
    domain::Order order(orderId, request.accountId, request.figi, 
                        request.direction, request.type, request.quantity, execPrice);
    
    if (request.type == domain::OrderType::MARKET) {
        order.updateStatus(domain::OrderStatus::FILLED);
        executeOrder(*account, order, *instrument);
        result.executedPrice = execPrice;
    } else {
        order.updateStatus(domain::OrderStatus::PENDING);
    }
    
    account->orders.insert(orderId, std::make_shared<domain::Order>(order));
    result.orderId = orderId;
    result.status = order.status;
    result.message = "OK";
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
    updated->updateStatus(domain::OrderStatus::CANCELLED);
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
    
    auto all = account->orders.getAll();
    std::vector<domain::Order> result;
    for (const auto& o : all) {
        result.push_back(*o);
    }
    return result;
}

std::vector<domain::Order> FakeTinkoffAdapter::getOrderHistory(
    const std::string& accountId,
    const std::optional<std::chrono::system_clock::time_point>& from,
    const std::optional<std::chrono::system_clock::time_point>& to
) {
    auto account = getAccount(accountId);
    if (!account) {
        return {};
    }
    
    // Упрощенная реализация - возвращаем все ордера
    return getOrders(accountId);
}

void FakeTinkoffAdapter::reset() {
    std::lock_guard<std::mutex> lock(accountsMutex_);
    accounts_.clear();
    initInstruments();
}

void FakeTinkoffAdapter::setCash(const std::string& accountId, const domain::Money& cash) {
    auto account = getAccount(accountId);
    if (!account) {
        return;
    }
    
    account->cash = cash;
}

void FakeTinkoffAdapter::setPositions(const std::string& accountId, const std::vector<domain::Position>& positions) {
    auto account = getOrCreateAccount(accountId);
    
    account->positions.clear();
    for (const auto& pos : positions) {
        account->positions.insert(pos.figi, std::make_shared<domain::Position>(pos));
    }
}

} // namespace trading::adapters::secondary