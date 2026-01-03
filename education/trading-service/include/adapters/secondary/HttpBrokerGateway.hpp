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
            
            // Broker может вернуть массив напрямую или объект с полем "quotes"
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
        // TODO: broker-service не поддерживает search endpoint.
        // Получаем все инструменты и фильтруем локально.
        // Рефакторинг: рассмотреть добавление search в broker-service.
        
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
            
            // Broker может вернуть массив напрямую или объект с полем "instruments"
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
            
            // Broker может вернуть массив напрямую или объект с полем "orders"
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
        
        auto cashJson = j.value("cash", nlohmann::json::object());
        portfolio.cash = domain::Money::fromDouble(
            cashJson.value("amount", 0.0),
            cashJson.value("currency", "RUB")
        );
        
        auto totalJson = j.value("total_value", nlohmann::json::object());
        portfolio.totalValue = domain::Money::fromDouble(
            totalJson.value("amount", 0.0),
            totalJson.value("currency", "RUB")
        );
        
        auto positions = j.value("positions", nlohmann::json::array());
        for (const auto& p : positions) {
            domain::Position pos;
            pos.figi = p.value("figi", "");
            pos.ticker = p.value("ticker", "");
            pos.quantity = p.value("quantity", 0);
            pos.averagePrice = domain::Money::fromDouble(
                p.value("average_price", 0.0),
                p.value("currency", "RUB")
            );
            pos.currentPrice = domain::Money::fromDouble(
                p.value("current_price", 0.0),
                p.value("currency", "RUB")
            );
            pos.pnl = domain::Money::fromDouble(
                p.value("pnl", 0.0),
                p.value("currency", "RUB")
            );
            pos.pnlPercent = p.value("pnl_percent", 0.0);
            portfolio.positions.push_back(pos);
        }
        
        return portfolio;
    }

    domain::Order parseOrder(const nlohmann::json& j) {
        domain::Order order(
            j.value("id", ""),
            j.value("account_id", ""),
            j.value("figi", ""),
            domain::parseDirection(j.value("direction", "BUY")),
            domain::parseOrderType(j.value("type", "MARKET")),
            j.value("quantity", 0),
            domain::Money::fromDouble(
                j.value("price", 0.0),
                j.value("currency", "RUB")
            )
        );
        order.updateStatus(domain::parseOrderStatus(j.value("status", "PENDING")));
        return order;
    }
};

} // namespace trading::adapters::secondary
