// trading-service/include/adapters/secondary/HttpBrokerGateway.hpp
#pragma once

#include "ports/output/IBrokerGateway.hpp"
#include "settings/IBrokerClientSettings.hpp"
#include <IHttpClient.hpp>
#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace trading::adapters::secondary {

/**
 * @brief HTTP клиент к Broker Service
 *
 * Реализует IBrokerGateway через HTTP запросы к broker-service.
 */
class HttpBrokerGateway : public ports::output::IBrokerGateway {
public:
    HttpBrokerGateway(
        std::shared_ptr<IHttpClient> httpClient,
        std::shared_ptr<settings::IBrokerClientSettings> settings
    ) : httpClient_(std::move(httpClient))
      , settings_(std::move(settings))
    {
        std::cout << "[HttpBrokerGateway] Created, target: "
                  << settings_->getHost() << ":" << settings_->getPort() << std::endl;
    }

    // ============================================
    // РЫНОЧНЫЕ ДАННЫЕ
    // ============================================

    std::optional<domain::Quote> getQuote(const std::string& figi) override {
        auto quotes = getQuotes({figi});
        if (quotes.empty()) {
            return std::nullopt;
        }
        return quotes[0];
    }

    std::vector<domain::Quote> getQuotes(const std::vector<std::string>& figis) override {
        std::vector<domain::Quote> result;
        
        try {
            std::string path = "/api/v1/quotes";
            if (!figis.empty()) {
                path += "?figis=";
                for (size_t i = 0; i < figis.size(); ++i) {
                    if (i > 0) path += ",";
                    path += figis[i];
                }
            }

            auto response = doGet(path);
            if (response.getStatus() != 200) {
                std::cerr << "[HttpBrokerGateway] getQuotes failed: " << response.getStatus() << std::endl;
                return result;
            }

            auto json = nlohmann::json::parse(response.getBody());
            
            nlohmann::json quotesJson;
            if (json.is_array()) {
                quotesJson = json;
            } else {
                quotesJson = json.value("quotes", nlohmann::json::array());
            }
            
            for (const auto& q : quotesJson) {
                result.push_back(parseQuote(q));
            }
        } catch (const std::exception& e) {
            std::cerr << "[HttpBrokerGateway] getQuotes error: " << e.what() << std::endl;
        }

        return result;
    }

    // ============================================
    // ИНСТРУМЕНТЫ
    // ============================================

    std::vector<domain::Instrument> searchInstruments(const std::string& query) override {
        auto allInstruments = getAllInstruments();
        
        if (query.empty()) {
            return allInstruments;
        }
        
        std::vector<domain::Instrument> result;
        std::string lowerQuery = toLower(query);
        
        for (const auto& instr : allInstruments) {
            if (toLower(instr.ticker).find(lowerQuery) != std::string::npos ||
                toLower(instr.name).find(lowerQuery) != std::string::npos) {
                result.push_back(instr);
            }
        }
        
        return result;
    }

    std::optional<domain::Instrument> getInstrumentByFigi(const std::string& figi) override {
        try {
            std::string path = "/api/v1/instruments/" + figi;
            auto response = doGet(path);
            
            if (response.getStatus() != 200) {
                return std::nullopt;
            }

            auto json = nlohmann::json::parse(response.getBody());
            return parseInstrument(json);
        } catch (const std::exception& e) {
            std::cerr << "[HttpBrokerGateway] getInstrumentByFigi error: " << e.what() << std::endl;
        }

        return std::nullopt;
    }

    std::vector<domain::Instrument> getAllInstruments() override {
        std::vector<domain::Instrument> result;
        
        try {
            auto response = doGet("/api/v1/instruments");
            
            if (response.getStatus() != 200) {
                return result;
            }

            auto json = nlohmann::json::parse(response.getBody());
            
            nlohmann::json instrumentsJson;
            if (json.is_array()) {
                instrumentsJson = json;
            } else {
                instrumentsJson = json.value("instruments", nlohmann::json::array());
            }
            
            for (const auto& i : instrumentsJson) {
                result.push_back(parseInstrument(i));
            }
        } catch (const std::exception& e) {
            std::cerr << "[HttpBrokerGateway] getAllInstruments error: " << e.what() << std::endl;
        }

        return result;
    }

    // ============================================
    // ДАННЫЕ АККАУНТА
    // ============================================

    domain::Portfolio getPortfolio(const std::string& accountId) override {
        domain::Portfolio portfolio;
        
        try {
            std::string path = "/api/v1/portfolio?account_id=" + accountId;
            auto response = doGet(path);
            
            if (response.getStatus() != 200) {
                throw std::runtime_error("Failed to get portfolio: " + std::to_string(response.getStatus()));
            }

            auto json = nlohmann::json::parse(response.getBody());
            portfolio = parsePortfolio(json);
        } catch (const std::exception& e) {
            std::cerr << "[HttpBrokerGateway] getPortfolio error: " << e.what() << std::endl;
            throw;
        }

        return portfolio;
    }

    std::vector<domain::Order> getOrders(const std::string& accountId) override {
        std::vector<domain::Order> result;
        
        try {
            std::string path = "/api/v1/orders?account_id=" + accountId;
            auto response = doGet(path);
            
            if (response.getStatus() != 200) {
                return result;
            }

            auto json = nlohmann::json::parse(response.getBody());
            
            nlohmann::json ordersJson;
            if (json.is_array()) {
                ordersJson = json;
            } else {
                ordersJson = json.value("orders", nlohmann::json::array());
            }
            
            for (const auto& o : ordersJson) {
                result.push_back(parseOrder(o));
            }
        } catch (const std::exception& e) {
            std::cerr << "[HttpBrokerGateway] getOrders error: " << e.what() << std::endl;
        }

        return result;
    }

    std::optional<domain::Order> getOrder(
        const std::string& accountId,
        const std::string& orderId
    ) override {
        try {
            std::string path = "/api/v1/orders/" + orderId + "?account_id=" + accountId;
            auto response = doGet(path);
            
            if (response.getStatus() != 200) {
                return std::nullopt;
            }

            auto json = nlohmann::json::parse(response.getBody());
            return parseOrder(json);
        } catch (const std::exception& e) {
            std::cerr << "[HttpBrokerGateway] getOrder error: " << e.what() << std::endl;
        }

        return std::nullopt;
    }

private:
    std::shared_ptr<IHttpClient> httpClient_;
    std::shared_ptr<settings::IBrokerClientSettings> settings_;

    static std::string toLower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    SimpleResponse doGet(const std::string& path) {
        SimpleRequest request(
            "GET",
            path,
            "",
            settings_->getHost(),
            settings_->getPort(),
            {}
        );

        SimpleResponse response;
        httpClient_->send(request, response);
        return response;
    }

    /**
     * @brief Парсинг Money из разных форматов broker'а
     * 
     * Broker может вернуть:
     * - {"units": 15958, "nano": 0, "currency": "RUB"}  (Tinkoff-style)
     * - {"amount": 159.58, "currency": "RUB"}          (простой формат)
     * - 159.58                                          (просто число)
     */
    domain::Money parseMoney(const nlohmann::json& j, const std::string& defaultCurrency = "RUB") {
        if (j.is_number()) {
            return domain::Money::fromDouble(j.get<double>(), defaultCurrency);
        }
        
        if (j.is_object()) {
            std::string currency = j.value("currency", defaultCurrency);
            
            // Tinkoff-style: units + nano
            if (j.contains("units")) {
                int64_t units = j.value("units", 0);
                int32_t nano = j.value("nano", 0);
                double amount = static_cast<double>(units) + static_cast<double>(nano) / 1e9;
                return domain::Money::fromDouble(amount, currency);
            }
            
            // Простой формат: amount
            if (j.contains("amount")) {
                return domain::Money::fromDouble(j.value("amount", 0.0), currency);
            }
        }
        
        return domain::Money::fromDouble(0.0, defaultCurrency);
    }

    domain::Quote parseQuote(const nlohmann::json& j) {
        std::string currency = j.value("currency", "RUB");
        return domain::Quote(
            j.value("figi", ""),
            j.value("ticker", ""),
            domain::Money::fromDouble(j.value("last_price", 0.0), currency),
            domain::Money::fromDouble(j.value("bid_price", 0.0), currency),
            domain::Money::fromDouble(j.value("ask_price", 0.0), currency)
        );
    }

    domain::Instrument parseInstrument(const nlohmann::json& j) {
        return domain::Instrument(
            j.value("figi", ""),
            j.value("ticker", ""),
            j.value("name", ""),
            j.value("currency", "RUB"),
            j.value("lot", 1)
        );
    }

    domain::Portfolio parsePortfolio(const nlohmann::json& j) {
        domain::Portfolio portfolio;
        
        // Cash - может быть в разных форматах
        if (j.contains("cash")) {
            portfolio.cash = parseMoney(j["cash"]);
        }
        
        // Total value
        if (j.contains("total_value")) {
            portfolio.totalValue = parseMoney(j["total_value"]);
        }
        
        // Positions
        auto positions = j.value("positions", nlohmann::json::array());
        for (const auto& p : positions) {
            domain::Position pos;
            pos.figi = p.value("figi", "");
            pos.ticker = p.value("ticker", "");
            pos.quantity = p.value("quantity", 0);
            
            std::string currency = p.value("currency", "RUB");
            pos.averagePrice = domain::Money::fromDouble(p.value("average_price", 0.0), currency);
            pos.currentPrice = domain::Money::fromDouble(p.value("current_price", 0.0), currency);
            pos.pnl = domain::Money::fromDouble(p.value("pnl", 0.0), currency);
            pos.pnlPercent = p.value("pnl_percent", 0.0);
            
            portfolio.positions.push_back(pos);
        }
        
        return portfolio;
    }

    domain::Order parseOrder(const nlohmann::json& j) {
        // order_id (broker) или id (legacy)
        std::string orderId = j.value("order_id", j.value("id", ""));
        
        // order_type (broker) или type (legacy)
        std::string orderType = j.value("order_type", j.value("type", "MARKET"));
        
        // price - может быть объектом или числом
        domain::Money price;
        if (j.contains("price")) {
            price = parseMoney(j["price"]);
        }
        
        domain::Order order(
            orderId,
            j.value("account_id", ""),
            j.value("figi", ""),
            domain::parseDirection(j.value("direction", "BUY")),
            domain::parseOrderType(orderType),
            j.value("quantity", 0),
            price
        );

        if (j.contains("executed_price")) {
            order.executedPrice = parseMoney(j["executed_price"]);
        }
        
        order.executedQuantity = j.value("executed_quantity", 
                                 j.value("filled_quantity",
                                 j.value("executed_lots", int64_t{0})));
        
        order.updateStatus(domain::parseOrderStatus(j.value("status", "PENDING")));
        return order;
    }
};

} // namespace trading::adapters::secondary
