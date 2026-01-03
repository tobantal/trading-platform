// include/adapters/secondary/PostgresQuoteRepository.hpp
#pragma once

#include "ports/output/IQuoteRepository.hpp"
#include "settings/DbSettings.hpp"
#include <pqxx/pqxx>
#include <memory>
#include <iostream>

namespace broker::adapters::secondary {

/**
 * @brief PostgreSQL реализация репозитория котировок
 * 
 * Таблица: quotes
 * - figi VARCHAR(12) PRIMARY KEY
 * - bid BIGINT NOT NULL (в копейках)
 * - ask BIGINT NOT NULL (в копейках)
 * - last_price BIGINT NOT NULL (в копейках)
 * - updated_at TIMESTAMP DEFAULT NOW()
 */
class PostgresQuoteRepository : public ports::output::IQuoteRepository {
public:
    explicit PostgresQuoteRepository(std::shared_ptr<settings::DbSettings> settings)
        : settings_(std::move(settings))
    {
        initSchema();
    }

    std::optional<domain::Quote> findByFigi(const std::string& figi) override {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            auto result = txn.exec_params(
                "SELECT q.figi, i.ticker, q.bid, q.ask, q.last_price, q.updated_at "
                "FROM quotes q "
                "LEFT JOIN instruments i ON q.figi = i.figi "
                "WHERE q.figi = $1",
                figi
            );
            
            if (result.empty()) {
                return std::nullopt;
            }
            
            domain::Quote quote;
            quote.figi = result[0]["figi"].as<std::string>();
            quote.ticker = result[0]["ticker"].is_null() ? figi : result[0]["ticker"].as<std::string>();
            
            // Конвертируем из копеек в Money
            int64_t bidCents = result[0]["bid"].as<int64_t>();
            int64_t askCents = result[0]["ask"].as<int64_t>();
            int64_t lastCents = result[0]["last_price"].as<int64_t>();
            
            quote.bidPrice = domain::Money(bidCents / 100, (bidCents % 100) * 10000000, "RUB");
            quote.askPrice = domain::Money(askCents / 100, (askCents % 100) * 10000000, "RUB");
            quote.lastPrice = domain::Money(lastCents / 100, (lastCents % 100) * 10000000, "RUB");
            
            return quote;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresQuoteRepository] findByFigi error: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    void save(const domain::Quote& quote) override {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            // Конвертируем Money в копейки
            int64_t bidCents = static_cast<int64_t>(quote.bidPrice.toDouble() * 100);
            int64_t askCents = static_cast<int64_t>(quote.askPrice.toDouble() * 100);
            int64_t lastCents = static_cast<int64_t>(quote.lastPrice.toDouble() * 100);
            
            txn.exec_params(
                "INSERT INTO quotes (figi, bid, ask, last_price, updated_at) "
                "VALUES ($1, $2, $3, $4, NOW()) "
                "ON CONFLICT (figi) DO UPDATE SET "
                "bid = EXCLUDED.bid, "
                "ask = EXCLUDED.ask, "
                "last_price = EXCLUDED.last_price, "
                "updated_at = NOW()",
                quote.figi,
                bidCents,
                askCents,
                lastCents
            );
            
            txn.commit();
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresQuoteRepository] save error: " << e.what() << std::endl;
            throw;
        }
    }
    
    /**
     * @brief Получить все котировки
     */
    std::vector<domain::Quote> findAll() {
        std::vector<domain::Quote> quotes;
        
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            auto result = txn.exec(
                "SELECT q.figi, i.ticker, q.bid, q.ask, q.last_price "
                "FROM quotes q "
                "LEFT JOIN instruments i ON q.figi = i.figi"
            );
            
            for (const auto& row : result) {
                domain::Quote quote;
                quote.figi = row["figi"].as<std::string>();
                quote.ticker = row["ticker"].is_null() ? quote.figi : row["ticker"].as<std::string>();
                
                int64_t bidCents = row["bid"].as<int64_t>();
                int64_t askCents = row["ask"].as<int64_t>();
                int64_t lastCents = row["last_price"].as<int64_t>();
                
                quote.bidPrice = domain::Money(bidCents / 100, (bidCents % 100) * 10000000, "RUB");
                quote.askPrice = domain::Money(askCents / 100, (askCents % 100) * 10000000, "RUB");
                quote.lastPrice = domain::Money(lastCents / 100, (lastCents % 100) * 10000000, "RUB");
                
                quotes.push_back(quote);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresQuoteRepository] findAll error: " << e.what() << std::endl;
        }
        
        return quotes;
    }

private:
    std::shared_ptr<settings::DbSettings> settings_;
    
    void initSchema() {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            txn.exec(R"(
                CREATE TABLE IF NOT EXISTS quotes (
                    figi VARCHAR(12) PRIMARY KEY,
                    bid BIGINT NOT NULL,
                    ask BIGINT NOT NULL,
                    last_price BIGINT NOT NULL,
                    updated_at TIMESTAMP DEFAULT NOW()
                )
            )");
            
            txn.commit();
            std::cout << "[PostgresQuoteRepository] Schema initialized" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresQuoteRepository] initSchema error: " << e.what() << std::endl;
        }
    }
};

} // namespace broker::adapters::secondary
