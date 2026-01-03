// include/adapters/secondary/PostgresInstrumentRepository.hpp
#pragma once

#include "ports/output/IInstrumentRepository.hpp"
#include "settings/DbSettings.hpp"
#include <pqxx/pqxx>
#include <memory>
#include <iostream>

namespace broker::adapters::secondary {

/**
 * @brief PostgreSQL реализация репозитория инструментов
 * 
 * Таблица: instruments
 * - figi VARCHAR(12) PRIMARY KEY
 * - ticker VARCHAR(12) NOT NULL
 * - name VARCHAR(128) NOT NULL
 * - currency VARCHAR(3) NOT NULL
 * - lot_size INTEGER NOT NULL
 * - min_price_increment BIGINT NOT NULL (в копейках)
 */
class PostgresInstrumentRepository : public ports::output::IInstrumentRepository {
public:
    explicit PostgresInstrumentRepository(std::shared_ptr<settings::DbSettings> settings)
        : settings_(std::move(settings))
    {
        initSchema();
        seedInstruments();
    }

    std::vector<domain::Instrument> findAll() override {
        std::vector<domain::Instrument> instruments;
        
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            auto result = txn.exec(
                "SELECT figi, ticker, name, currency, lot_size, min_price_increment "
                "FROM instruments ORDER BY ticker"
            );
            
            for (const auto& row : result) {
                domain::Instrument instr;
                instr.figi = row["figi"].as<std::string>();
                instr.ticker = row["ticker"].as<std::string>();
                instr.name = row["name"].as<std::string>();
                instr.currency = row["currency"].as<std::string>();
                instr.lot = row["lot_size"].as<int>();
                
                int64_t minIncCents = row["min_price_increment"].as<int64_t>();
                instr.minPriceIncrement = domain::Money(minIncCents / 100, (minIncCents % 100) * 10000000, instr.currency);
                
                instruments.push_back(instr);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresInstrumentRepository] findAll error: " << e.what() << std::endl;
        }
        
        return instruments;
    }

    std::optional<domain::Instrument> findByFigi(const std::string& figi) override {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            auto result = txn.exec_params(
                "SELECT figi, ticker, name, currency, lot_size, min_price_increment "
                "FROM instruments WHERE figi = $1",
                figi
            );
            
            if (result.empty()) {
                return std::nullopt;
            }
            
            domain::Instrument instr;
            instr.figi = result[0]["figi"].as<std::string>();
            instr.ticker = result[0]["ticker"].as<std::string>();
            instr.name = result[0]["name"].as<std::string>();
            instr.currency = result[0]["currency"].as<std::string>();
            instr.lot = result[0]["lot_size"].as<int>();
            
            int64_t minIncCents = result[0]["min_price_increment"].as<int64_t>();
            instr.minPriceIncrement = domain::Money(minIncCents / 100, (minIncCents % 100) * 10000000, instr.currency);
            
            return instr;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresInstrumentRepository] findByFigi error: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    void save(const domain::Instrument& instrument) override {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            int64_t minIncCents = static_cast<int64_t>(instrument.minPriceIncrement.toDouble() * 100);
            
            txn.exec_params(
                "INSERT INTO instruments (figi, ticker, name, currency, lot_size, min_price_increment) "
                "VALUES ($1, $2, $3, $4, $5, $6) "
                "ON CONFLICT (figi) DO UPDATE SET "
                "ticker = EXCLUDED.ticker, "
                "name = EXCLUDED.name, "
                "currency = EXCLUDED.currency, "
                "lot_size = EXCLUDED.lot_size, "
                "min_price_increment = EXCLUDED.min_price_increment",
                instrument.figi,
                instrument.ticker,
                instrument.name,
                instrument.currency,
                instrument.lot,
                minIncCents
            );
            
            txn.commit();
            std::cout << "[PostgresInstrumentRepository] Saved instrument " << instrument.ticker << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresInstrumentRepository] save error: " << e.what() << std::endl;
            throw;
        }
    }
    
    /**
     * @brief Поиск инструментов по тикеру/названию
     */
    std::vector<domain::Instrument> search(const std::string& query) {
        std::vector<domain::Instrument> instruments;
        
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            std::string pattern = "%" + query + "%";
            
            auto result = txn.exec_params(
                "SELECT figi, ticker, name, currency, lot_size, min_price_increment "
                "FROM instruments "
                "WHERE LOWER(ticker) LIKE LOWER($1) OR LOWER(name) LIKE LOWER($1) "
                "ORDER BY ticker",
                pattern
            );
            
            for (const auto& row : result) {
                domain::Instrument instr;
                instr.figi = row["figi"].as<std::string>();
                instr.ticker = row["ticker"].as<std::string>();
                instr.name = row["name"].as<std::string>();
                instr.currency = row["currency"].as<std::string>();
                instr.lot = row["lot_size"].as<int>();
                
                int64_t minIncCents = row["min_price_increment"].as<int64_t>();
                instr.minPriceIncrement = domain::Money(minIncCents / 100, (minIncCents % 100) * 10000000, instr.currency);
                
                instruments.push_back(instr);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresInstrumentRepository] search error: " << e.what() << std::endl;
        }
        
        return instruments;
    }

private:
    std::shared_ptr<settings::DbSettings> settings_;
    
    void initSchema() {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            txn.exec(R"(
                CREATE TABLE IF NOT EXISTS instruments (
                    figi VARCHAR(12) PRIMARY KEY,
                    ticker VARCHAR(12) NOT NULL,
                    name VARCHAR(128) NOT NULL,
                    currency VARCHAR(3) NOT NULL,
                    lot_size INTEGER NOT NULL,
                    min_price_increment BIGINT NOT NULL
                )
            )");
            
            txn.commit();
            std::cout << "[PostgresInstrumentRepository] Schema initialized" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresInstrumentRepository] initSchema error: " << e.what() << std::endl;
        }
    }
    
    void seedInstruments() {
        try {
            pqxx::connection conn(settings_->getConnectionString());
            pqxx::work txn(conn);
            
            // Проверяем есть ли данные
            auto count = txn.exec("SELECT COUNT(*) FROM instruments");
            if (count[0][0].as<int>() > 0) {
                return;  // Уже есть данные
            }
            
            // Seed данные - русские названия
            txn.exec(R"(
                INSERT INTO instruments (figi, ticker, name, currency, lot_size, min_price_increment) VALUES
                ('BBG004730N88', 'SBER', 'Сбербанк', 'RUB', 10, 100),
                ('BBG004730RP0', 'GAZP', 'Газпром', 'RUB', 10, 100),
                ('BBG004731032', 'LKOH', 'Лукойл', 'RUB', 1, 500),
                ('BBG004731354', 'ROSN', 'Роснефть', 'RUB', 1, 100),
                ('BBG004S68614', 'ALRS', 'Алроса', 'RUB', 10, 10),
                ('BBG004730ZJ9', 'VTBR', 'ВТБ', 'RUB', 10000, 1),
                ('BBG006L8G4H1', 'YNDX', 'Яндекс', 'RUB', 1, 200)
                ON CONFLICT (figi) DO NOTHING
            )");
            
            // Seed котировки
            txn.exec(R"(
                INSERT INTO quotes (figi, bid, ask, last_price) VALUES
                ('BBG004730N88', 26500, 26550, 26525),
                ('BBG004730RP0', 15800, 15850, 15820),
                ('BBG004731032', 710000, 710500, 710200),
                ('BBG004731354', 57500, 57600, 57550),
                ('BBG004S68614', 7200, 7250, 7220),
                ('BBG004730ZJ9', 2, 3, 2),
                ('BBG006L8G4H1', 350000, 350500, 350200)
                ON CONFLICT (figi) DO NOTHING
            )");
            
            txn.commit();
            std::cout << "[PostgresInstrumentRepository] Seed data inserted" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresInstrumentRepository] seedInstruments error: " << e.what() << std::endl;
        }
    }
};

} // namespace broker::adapters::secondary
