#include "adapters/secondary/broker/FakeTinkoffAdapter.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace trading::adapters::secondary {

FakeTinkoffAdapter::FakeTinkoffAdapter() 
    : cash_(domain::Money::fromDouble(1000000.0, "RUB"))
    , rng_(std::random_device{}()) 
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

void FakeTinkoffAdapter::reset() {
    positions_.clear();
    orders_.clear();
    cash_ = domain::Money::fromDouble(1000000.0, "RUB");
}

void FakeTinkoffAdapter::setCash(const domain::Money& cash) {
    cash_ = cash;
}

void FakeTinkoffAdapter::setAccessToken(const std::string& token) {
    accessToken_ = token;
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

domain::Portfolio FakeTinkoffAdapter::getPortfolio() {
    domain::Portfolio portfolio;
    portfolio.cash = cash_;
    portfolio.positions = getPositions();
    portfolio.recalculateTotalValue();
    return portfolio;
}

std::vector<domain::Position> FakeTinkoffAdapter::getPositions() {
    auto all = positions_.getAll();
    std::vector<domain::Position> result;
    for (const auto& pos : all) {
        domain::Position updated = *pos;
        if (auto quote = getQuote(updated.figi)) {
            updated.updateCurrentPrice(quote->lastPrice);
        }
        result.push_back(updated);
    }
    return result;
}

domain::Money FakeTinkoffAdapter::getCash() {
    return cash_;
}

domain::OrderResult FakeTinkoffAdapter::placeOrder(const domain::OrderRequest& request) {
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
    
    if (request.direction == domain::OrderDirection::BUY && totalCost > cash_) {
        result.status = domain::OrderStatus::REJECTED;
        result.message = "Insufficient funds";
        return result;
    }
    
    if (request.direction == domain::OrderDirection::SELL) {
        auto pos = positions_.find(request.figi);
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
        
        if (request.direction == domain::OrderDirection::BUY) {
            cash_ = cash_ - totalCost;
            auto existing = positions_.find(request.figi);
            if (existing) {
                int64_t newQty = existing->quantity + request.quantity;
                double avgPrice = (existing->averagePrice.toDouble() * existing->quantity +
                                   execPrice.toDouble() * request.quantity) / newQty;
                positions_.insert(request.figi, std::make_shared<domain::Position>(
                    request.figi, instrument->ticker, newQty,
                    domain::Money::fromDouble(avgPrice, "RUB"), execPrice));
            } else {
                positions_.insert(request.figi, std::make_shared<domain::Position>(
                    request.figi, instrument->ticker, request.quantity, execPrice, execPrice));
            }
        } else {
            cash_ = cash_ + totalCost;
            auto existing = positions_.find(request.figi);
            int64_t newQty = existing->quantity - request.quantity;
            if (newQty > 0) {
                positions_.insert(request.figi, std::make_shared<domain::Position>(
                    request.figi, instrument->ticker, newQty, existing->averagePrice, execPrice));
            } else {
                positions_.remove(request.figi);
            }
        }
        result.executedPrice = execPrice;
    } else {
        order.updateStatus(domain::OrderStatus::PENDING);
    }
    
    orders_.insert(orderId, std::make_shared<domain::Order>(order));
    result.orderId = orderId;
    result.status = order.status;
    result.message = "OK";
    return result;
}

bool FakeTinkoffAdapter::cancelOrder(const std::string& orderId) {
    auto order = orders_.find(orderId);
    if (!order || order->status != domain::OrderStatus::PENDING) {
        return false;
    }
    auto updated = std::make_shared<domain::Order>(*order);
    updated->updateStatus(domain::OrderStatus::CANCELLED);
    orders_.insert(orderId, updated);
    return true;
}

std::optional<domain::Order> FakeTinkoffAdapter::getOrderStatus(const std::string& orderId) {
    auto order = orders_.find(orderId);
    return order ? std::optional(*order) : std::nullopt;
}

std::vector<domain::Order> FakeTinkoffAdapter::getOrders() {
    auto all = orders_.getAll();
    std::vector<domain::Order> result;
    for (const auto& o : all) {
        result.push_back(*o);
    }
    return result;
}

} // namespace trading::adapters::secondary