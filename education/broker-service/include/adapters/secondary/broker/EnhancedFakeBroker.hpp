// include/adapters/secondary/broker/EnhancedFakeBroker.hpp
#pragma once

#include "MarketScenario.hpp"
#include "PriceSimulator.hpp"
#include "OrderProcessor.hpp"
#include "BackgroundTicker.hpp"
#include "settings/BrokerSettings.hpp"

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace broker::adapters::secondary {

// ============================================================================
// DOMAIN TYPES (для изоляции от основного domain)
// ============================================================================

/**
 * @brief Котировка (совместимая с domain::Quote)
 */
struct BrokerQuote {
    std::string figi;
    std::string ticker;
    double lastPrice = 0.0;
    double bidPrice = 0.0;
    double askPrice = 0.0;
    int64_t volume = 0;
    
    double spread() const { return askPrice - bidPrice; }
    double mid() const { return (bidPrice + askPrice) / 2.0; }
};

/**
 * @brief Инструмент
 */
struct BrokerInstrument {
    std::string figi;
    std::string ticker;
    std::string name;
    std::string currency = "RUB";
    int lot = 1;
    double minPriceIncrement = 0.01;
};

/**
 * @brief Запрос на ордер (совместимый с domain::OrderRequest)
 */
struct BrokerOrderRequest {
    std::string accountId;
    std::string figi;
    Direction direction = Direction::BUY;
    Type type = Type::MARKET;
    int64_t quantity = 0;
    double price = 0.0;
};

/**
 * @brief Результат ордера (совместимый с domain::OrderResult)
 */
struct BrokerOrderResult {
    std::string orderId;
    Status status = Status::PENDING;
    double executedPrice = 0.0;
    int64_t executedQuantity = 0;
    std::string message;
    
    bool isSuccess() const {
        return status == Status::FILLED || status == Status::PARTIALLY_FILLED;
    }
};

/**
 * @brief Позиция
 */
struct BrokerPosition {
    std::string figi;
    std::string ticker;
    int64_t quantity = 0;
    double averagePrice = 0.0;
    double currentPrice = 0.0;
    
    double totalValue() const { return quantity * currentPrice; }
    double pnl() const { return quantity * (currentPrice - averagePrice); }
    double pnlPercent() const {
        if (averagePrice == 0) return 0;
        return (currentPrice - averagePrice) / averagePrice * 100.0;
    }
};

/**
 * @brief Портфель
 */
struct BrokerPortfolio {
    std::string accountId;
    double cash = 0.0;
    std::string currency = "RUB";
    std::vector<BrokerPosition> positions;
    
    double totalValue() const {
        double total = cash;
        for (const auto& pos : positions) {
            total += pos.totalValue();
        }
        return total;
    }
};

// ============================================================================
// CALLBACKS
// ============================================================================

/**
 * @brief Событие исполнения ордера
 */
struct BrokerOrderFillEvent {
    std::string orderId;
    std::string accountId;
    std::string figi;
    Direction direction;
    int64_t quantity;
    double price;
    bool partial;
};

/**
 * @brief Событие обновления котировки
 */
struct BrokerQuoteUpdateEvent {
    std::string figi;
    double bid;
    double ask;
    double last;
    int64_t volume;
};

using OrderFillEventCallback = std::function<void(const BrokerOrderFillEvent&)>;
using QuoteUpdateEventCallback = std::function<void(const BrokerQuoteUpdateEvent&)>;

// ============================================================================
// ENHANCED FAKE BROKER
// ============================================================================

/**
 * @brief Улучшенный фейковый брокер с реалистичной симуляцией
 * 
 * Объединяет:
 * - PriceSimulator: генерация цен с random walk
 * - OrderProcessor: обработка ордеров со сценариями
 * - BackgroundTicker: фоновое обновление цен
 * 
 * Поддерживает:
 * - Множество аккаунтов с изолированными портфелями
 * - Различные сценарии исполнения (slippage, partial fills, rejection)
 * - События для интеграции с EventBus
 * - Детерминированное тестирование (через setPrice)
 */
class EnhancedFakeBroker {
public:
    /**
     * @brief Конструктор
     * @param settings Настройки брокера (seed, slippage и т.д.)
     */
    explicit EnhancedFakeBroker(std::shared_ptr<settings::BrokerSettings> settings)
        : settings_(std::move(settings))
        , priceSimulator_(std::make_shared<PriceSimulator>(settings_->getSeed()))
        , orderProcessor_(std::make_shared<OrderProcessor>(priceSimulator_))
        , ticker_(std::make_shared<BackgroundTicker>(priceSimulator_, orderProcessor_))
    {
        initDefaultInstruments();
        setupCallbacks();
        
        std::cout << "[EnhancedFakeBroker] Initialized with seed=" << settings_->getSeed() << std::endl;
    }
    
    ~EnhancedFakeBroker() {
        stopSimulation();
    }
    
    // ========================================================================
    // SIMULATION CONTROL
    // ========================================================================
    
    /**
     * @brief Запустить фоновую симуляцию цен
     */
    void startSimulation(std::chrono::milliseconds interval = std::chrono::milliseconds{100}) {
        ticker_->setInterval(interval);
        ticker_->start();
    }
    
    /**
     * @brief Остановить симуляцию
     */
    void stopSimulation() {
        ticker_->stop();
    }
    
    /**
     * @brief Проверить, запущена ли симуляция
     */
    bool isSimulationRunning() const {
        return ticker_->isRunning();
    }
    
    /**
     * @brief Выполнить один тик вручную
     */
    void manualTick() {
        ticker_->manualTick();
    }
    
    // ========================================================================
    // SCENARIO CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Установить сценарий для инструмента
     */
    void setScenario(const std::string& figi, const MarketScenario& scenario) {
        std::lock_guard<std::mutex> lock(mutex_);
        scenarios_[figi] = scenario;
        priceSimulator_->initInstrument(
            figi,
            scenario.basePrice,
            scenario.bidAskSpread,
            scenario.volatility);
        ticker_->addInstrument(figi);
    }
    
    /**
     * @brief Установить сценарий по умолчанию
     */
    void setDefaultScenario(const MarketScenario& scenario) {
        std::lock_guard<std::mutex> lock(mutex_);
        defaultScenario_ = scenario;
    }
    
    /**
     * @brief Получить сценарий для инструмента
     */
    MarketScenario getScenario(const std::string& figi) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = scenarios_.find(figi);
        return (it != scenarios_.end()) ? it->second : defaultScenario_;
    }
    
    // ========================================================================
    // PRICE MANIPULATION (для тестов)
    // ========================================================================
    
    /**
     * @brief Установить конкретную цену
     */
    void setPrice(const std::string& figi, double price) {
        priceSimulator_->setPrice(figi, price);
    }
    
    /**
     * @brief Сдвинуть цену
     */
    void movePrice(const std::string& figi, double delta) {
        priceSimulator_->movePrice(figi, delta);
    }
    
    /**
     * @brief Изменить цену на процент
     */
    void movePricePercent(const std::string& figi, double percent) {
        priceSimulator_->movePricePercent(figi, percent);
    }
    
    /**
     * @brief Получить симулятор цен (для прямого управления)
     */
    PriceSimulator& priceSimulator() { return *priceSimulator_; }
    
    /**
     * @brief Получить процессор ордеров (для тестов)
     */
    OrderProcessor& orderProcessor() { return *orderProcessor_; }
    
    // ========================================================================
    // EVENT CALLBACKS
    // ========================================================================
    
    /**
     * @brief Установить callback для событий исполнения ордеров
     */
    void setOrderFillCallback(OrderFillEventCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        orderFillCallback_ = std::move(callback);
    }
    
    /**
     * @brief Установить callback для обновления котировок
     */
    void setQuoteUpdateCallback(QuoteUpdateEventCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        quoteUpdateCallback_ = std::move(callback);
        
        // Пробрасываем в тикер
        ticker_->setQuoteCallback([this](const QuoteUpdate& update) {
            QuoteUpdateEventCallback cb;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                cb = quoteUpdateCallback_;
            }
            if (cb) {
                BrokerQuoteUpdateEvent event;
                event.figi = update.figi;
                event.bid = update.bid;
                event.ask = update.ask;
                event.last = update.last;
                event.volume = update.volume;
                cb(event);
            }
        });
    }
    
    // ========================================================================
    // ACCOUNT MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Зарегистрировать аккаунт
     */
    void registerAccount(const std::string& accountId, const std::string& token, double initialCash = 100000.0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (accounts_.find(accountId) == accounts_.end()) {
            AccountData account;
            account.token = token;
            account.cash = initialCash;
            accounts_[accountId] = account;
        }
    }
    
    /**
     * @brief Удалить аккаунт
     */
    void unregisterAccount(const std::string& accountId) {
        std::lock_guard<std::mutex> lock(mutex_);
        accounts_.erase(accountId);
    }
    
    /**
     * @brief Проверить существование аккаунта
     */
    bool hasAccount(const std::string& accountId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return accounts_.find(accountId) != accounts_.end();
    }
    
    /**
     * @brief Импортировать позицию (для восстановления из БД)
     */
    void importPosition(const std::string& accountId, const std::string& figi, 
                        const std::string& ticker, int64_t quantity, double averagePrice) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = accounts_.find(accountId);
        if (it != accounts_.end()) {
            PositionData pos;
            pos.ticker = ticker;
            pos.quantity = quantity;
            pos.averagePrice = averagePrice;
            it->second.positions[figi] = pos;
        }
    }
    
    // ========================================================================
    // MARKET DATA
    // ========================================================================
    
    /**
     * @brief Получить котировку
     */
    std::optional<BrokerQuote> getQuote(const std::string& figi) const {
        auto quote = priceSimulator_->getQuote(figi);
        if (!quote) return std::nullopt;
        
        BrokerQuote result;
        result.figi = figi;
        result.lastPrice = quote->last;
        result.bidPrice = quote->bid;
        result.askPrice = quote->ask;
        result.volume = quote->volume;
        
        // Ticker из инструмента
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = instruments_.find(figi);
        if (it != instruments_.end()) {
            result.ticker = it->second.ticker;
        }
        
        return result;
    }
    
    /**
     * @brief Получить инструмент
     */
    std::optional<BrokerInstrument> getInstrument(const std::string& figi) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = instruments_.find(figi);
        if (it != instruments_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Получить все инструменты
     */
    std::vector<BrokerInstrument> getAllInstruments() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<BrokerInstrument> result;
        result.reserve(instruments_.size());
        for (const auto& [figi, instr] : instruments_) {
            result.push_back(instr);
        }
        return result;
    }
    
    /**
     * @brief Поиск инструментов по query
     */
    std::vector<BrokerInstrument> searchInstruments(const std::string& query) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<BrokerInstrument> result;
        
        for (const auto& [figi, instr] : instruments_) {
            if (instr.ticker.find(query) != std::string::npos ||
                instr.name.find(query) != std::string::npos ||
                instr.figi.find(query) != std::string::npos) {
                result.push_back(instr);
            }
        }
        return result;
    }
    
    // ========================================================================
    // PORTFOLIO
    // ========================================================================
    
    /**
     * @brief Получить портфель аккаунта
     */
    BrokerPortfolio getPortfolio(const std::string& accountId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        BrokerPortfolio portfolio;
        portfolio.accountId = accountId;
        
        auto it = accounts_.find(accountId);
        if (it == accounts_.end()) {
            return portfolio;
        }
        
        const auto& account = it->second;
        portfolio.cash = account.cash;
        
        for (const auto& [figi, posData] : account.positions) {
            BrokerPosition pos;
            pos.figi = figi;
            pos.ticker = posData.ticker;
            pos.quantity = posData.quantity;
            pos.averagePrice = posData.averagePrice;
            
            // Текущая цена из симулятора (без блокировки - уже под mutex_)
            auto quote = priceSimulator_->getQuote(figi);
            if (quote) {
                pos.currentPrice = quote->last;
            } else {
                pos.currentPrice = pos.averagePrice;
            }
            
            portfolio.positions.push_back(pos);
        }
        
        return portfolio;
    }
    
    /**
     * @brief Получить баланс
     */
    double getBalance(const std::string& accountId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = accounts_.find(accountId);
        if (it != accounts_.end()) {
            return it->second.cash;
        }
        return 0.0;
    }
    
    /**
     * @brief Установить баланс аккаунта (для тестов)
     */
    void setCash(const std::string& accountId, double cash) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = accounts_.find(accountId);
        if (it != accounts_.end()) {
            it->second.cash = cash;
        }
    }
    
    // ========================================================================
    // ORDERS
    // ========================================================================
    
    /**
     * @brief Разместить ордер
     */
    BrokerOrderResult placeOrder(const std::string& accountId, const BrokerOrderRequest& request) {
        // 1. Проверяем аккаунт
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (accounts_.find(accountId) == accounts_.end()) {
                BrokerOrderResult result;
                result.status = Status::REJECTED;
                result.message = "Account not found";
                return result;
            }
        }
        
        // 2. Проверяем инструмент
        auto instrOpt = getInstrument(request.figi);
        if (!instrOpt) {
            BrokerOrderResult result;
            result.status = Status::REJECTED;
            result.message = "Instrument not found: " + request.figi;
            return result;
        }
        
        // 3. Проверяем баланс для покупки
        if (request.direction == Direction::BUY) {
            auto quote = getQuote(request.figi);
            double price = (request.type == Type::MARKET && quote)
                ? quote->askPrice
                : request.price;
            double totalCost = price * request.quantity * instrOpt->lot;
            
            std::lock_guard<std::mutex> lock(mutex_);
            if (accounts_[accountId].cash < totalCost) {
                BrokerOrderResult result;
                result.status = Status::REJECTED;
                result.message = "Insufficient funds";
                return result;
            }
        }
        
        // 3.5. Проверяем позицию для продажи (короткие продажи запрещены)
        if (request.direction == Direction::SELL) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto& account = accounts_[accountId];
            auto posIt = account.positions.find(request.figi);
            
            int64_t availableQuantity = 0;
            if (posIt != account.positions.end()) {
                availableQuantity = posIt->second.quantity / instrOpt->lot;  // в лотах
            }
            
            if (availableQuantity < request.quantity) {
                BrokerOrderResult result;
                result.status = Status::REJECTED;
                result.message = "Insufficient position: have " + 
                    std::to_string(availableQuantity) + " lots, need " + 
                    std::to_string(request.quantity);
                return result;
            }
        }
        
        // 4. Обрабатываем ордер
        OrderRequest procRequest;
        procRequest.accountId = accountId;
        procRequest.figi = request.figi;
        procRequest.direction = request.direction;
        procRequest.type = request.type;
        procRequest.quantity = request.quantity;
        procRequest.price = request.price;
        
        auto scenario = getScenario(request.figi);
        auto procResult = orderProcessor_->processOrder(procRequest, scenario);
        
        // 5. Если исполнен — обновляем портфель
        if (procResult.status == Status::FILLED || procResult.status == Status::PARTIALLY_FILLED) {
            executeOrder(accountId, request, *instrOpt, procResult);
        }
        
        // 6. Конвертируем результат
        BrokerOrderResult result;
        result.orderId = procResult.orderId;
        result.status = procResult.status;
        result.executedPrice = procResult.executedPrice;
        result.executedQuantity = procResult.executedQuantity;
        result.message = procResult.message;
        
        return result;
    }
    
    /**
     * @brief Отменить ордер
     */
    bool cancelOrder(const std::string& accountId, const std::string& orderId) {
        return orderProcessor_->cancelOrder(orderId);
    }
    
    /**
     * @brief Получить количество pending ордеров
     */
    size_t pendingOrderCount() const {
        return orderProcessor_->pendingCount();
    }
    
    // ========================================================================
    // RESET (для тестов)
    // ========================================================================
    
    /**
     * @brief Сбросить состояние брокера
     */
    void reset() {
        stopSimulation();
        
        std::lock_guard<std::mutex> lock(mutex_);
        accounts_.clear();
        scenarios_.clear();
        orderProcessor_->clearPending();
        priceSimulator_->clear();
        
        initDefaultInstruments();
    }

private:
    std::shared_ptr<settings::BrokerSettings> settings_;
    std::shared_ptr<PriceSimulator> priceSimulator_;
    std::shared_ptr<OrderProcessor> orderProcessor_;
    std::shared_ptr<BackgroundTicker> ticker_;
    
    mutable std::mutex mutex_;
    
    // Сценарии
    MarketScenario defaultScenario_;
    std::unordered_map<std::string, MarketScenario> scenarios_;
    
    // Инструменты
    std::unordered_map<std::string, BrokerInstrument> instruments_;
    
    // Аккаунты
    struct PositionData {
        std::string ticker;
        int64_t quantity = 0;
        double averagePrice = 0.0;
    };
    
    struct AccountData {
        std::string token;
        double cash = 0.0;
        std::unordered_map<std::string, PositionData> positions;
    };
    
    std::unordered_map<std::string, AccountData> accounts_;
    
    // Callbacks
    OrderFillEventCallback orderFillCallback_;
    QuoteUpdateEventCallback quoteUpdateCallback_;
    
    void initDefaultInstruments() {
        // SBER
        instruments_["BBG004730N88"] = {"BBG004730N88", "SBER", "Сбербанк", "RUB", 10, 0.01};
        priceSimulator_->initInstrument("BBG004730N88", 280.0, 0.001, 0.002);
        ticker_->addInstrument("BBG004730N88");
        
        // GAZP
        instruments_["BBG004730RP0"] = {"BBG004730RP0", "GAZP", "Газпром", "RUB", 10, 0.01};
        priceSimulator_->initInstrument("BBG004730RP0", 160.0, 0.001, 0.003);
        ticker_->addInstrument("BBG004730RP0");
        
        // YNDX
        instruments_["BBG006L8G4H1"] = {"BBG006L8G4H1", "YNDX", "Яндекс", "RUB", 1, 0.1};
        priceSimulator_->initInstrument("BBG006L8G4H1", 3500.0, 0.002, 0.004);
        ticker_->addInstrument("BBG006L8G4H1");
        
        // LKOH
        instruments_["BBG004731032"] = {"BBG004731032", "LKOH", "Лукойл", "RUB", 1, 0.5};
        priceSimulator_->initInstrument("BBG004731032", 7200.0, 0.001, 0.002);
        ticker_->addInstrument("BBG004731032");
        
        // MGNT
        instruments_["BBG004RVFCY3"] = {"BBG004RVFCY3", "MGNT", "Магнит", "RUB", 1, 0.5};
        priceSimulator_->initInstrument("BBG004RVFCY3", 5500.0, 0.002, 0.003);
        ticker_->addInstrument("BBG004RVFCY3");
    }
    
    void setupCallbacks() {
        // Callback от OrderProcessor при исполнении pending ордеров
        orderProcessor_->setFillCallback([this](const OrderFillEvent& e) {
            // Обновляем портфель
            auto instrOpt = getInstrument(e.figi);
            if (instrOpt) {
                BrokerOrderRequest req;
                req.accountId = e.accountId;
                req.figi = e.figi;
                req.quantity = e.quantity;
                req.direction = e.direction;
                
                OrderResult procResult;
                procResult.orderId = e.orderId;
                procResult.executedPrice = e.price;
                procResult.executedQuantity = e.quantity;
                procResult.status = e.partial ? Status::PARTIALLY_FILLED : Status::FILLED;
                
                executeOrder(e.accountId, req, *instrOpt, procResult);
            }
            
            // Вызываем внешний callback
            OrderFillEventCallback cb;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                cb = orderFillCallback_;
            }
            if (cb) {
                BrokerOrderFillEvent event;
                event.orderId = e.orderId;
                event.accountId = e.accountId;
                event.figi = e.figi;
                event.direction = e.direction;
                event.quantity = e.quantity;
                event.price = e.price;
                event.partial = e.partial;
                cb(event);
            }
        });
    }
    
    void executeOrder(
        const std::string& accountId,
        const BrokerOrderRequest& request,
        const BrokerInstrument& instrument,
        const OrderResult& result)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = accounts_.find(accountId);
        if (it == accounts_.end()) return;
        
        auto& account = it->second;
        int64_t totalShares = result.executedQuantity * instrument.lot;
        double totalCost = result.executedPrice * totalShares;
        
        if (request.direction == Direction::BUY) {
            account.cash -= totalCost;
            
            auto& pos = account.positions[request.figi];
            if (pos.quantity == 0) {
                pos.ticker = instrument.ticker;
                pos.averagePrice = result.executedPrice;
                pos.quantity = totalShares;
            } else {
                // Средняя цена
                double newAvg = (pos.averagePrice * pos.quantity + 
                                result.executedPrice * totalShares) /
                               (pos.quantity + totalShares);
                pos.averagePrice = newAvg;
                pos.quantity += totalShares;
            }
        } else {
            account.cash += totalCost;
            
            auto posIt = account.positions.find(request.figi);
            if (posIt != account.positions.end()) {
                posIt->second.quantity -= totalShares;
                if (posIt->second.quantity <= 0) {
                    account.positions.erase(posIt);
                }
            }
        }
    }
};

} // namespace broker::adapters::secondary
