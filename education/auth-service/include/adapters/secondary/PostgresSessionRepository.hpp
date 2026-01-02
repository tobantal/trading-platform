#pragma once

#include "ports/output/ISessionRepository.hpp"
#include "DbSettings.hpp"
#include <pqxx/pqxx>
#include <memory>
#include <mutex>
#include <iostream>

namespace auth::adapters::secondary {

class PostgresSessionRepository : public ports::output::ISessionRepository {
public:
    explicit PostgresSessionRepository(std::shared_ptr<DbSettings> settings)
        : settings_(std::move(settings))
    {
        std::cout << "[PostgresSessionRepository] Connecting..." << std::endl;
        try {
            connection_ = std::make_unique<pqxx::connection>(settings_->getConnectionString());
            std::cout << "[PostgresSessionRepository] Connected" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[PostgresSessionRepository] Connection failed: " << e.what() << std::endl;
            throw;
        }
    }

    ~PostgresSessionRepository() {
        if (connection_ && connection_->is_open()) {
            connection_->close();
        }
    }

    domain::Session save(const domain::Session& session) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto expSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                session.expiresAt.time_since_epoch()).count();
            
            txn.exec_params(
                R"(
                    INSERT INTO sessions (session_id, user_id, jwt_token, expires_at, created_at)
                    VALUES ($1, $2, $3, to_timestamp($4), NOW())
                    ON CONFLICT (session_id) DO UPDATE SET
                        jwt_token = EXCLUDED.jwt_token,
                        expires_at = EXCLUDED.expires_at
                )",
                session.sessionId,
                session.userId,
                session.jwtToken,
                expSeconds
            );
            
            txn.commit();
            return session;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresSessionRepository] save() failed: " << e.what() << std::endl;
            throw;
        }
    }

    std::optional<domain::Session> findById(const std::string& sessionId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                R"(SELECT session_id, user_id, jwt_token, 
                          EXTRACT(EPOCH FROM expires_at)::bigint as exp_epoch
                   FROM sessions WHERE session_id = $1)",
                sessionId
            );
            
            txn.commit();
            
            if (result.empty()) return std::nullopt;
            
            return rowToSession(result[0]);
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresSessionRepository] findById() failed: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    std::optional<domain::Session> findByToken(const std::string& jwtToken) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                R"(SELECT session_id, user_id, jwt_token,
                          EXTRACT(EPOCH FROM expires_at)::bigint as exp_epoch
                   FROM sessions WHERE jwt_token = $1)",
                jwtToken
            );
            
            txn.commit();
            
            if (result.empty()) return std::nullopt;
            
            return rowToSession(result[0]);
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresSessionRepository] findByToken() failed: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    std::vector<domain::Session> findByUserId(const std::string& userId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                R"(SELECT session_id, user_id, jwt_token,
                          EXTRACT(EPOCH FROM expires_at)::bigint as exp_epoch
                   FROM sessions WHERE user_id = $1)",
                userId
            );
            
            txn.commit();
            
            std::vector<domain::Session> sessions;
            for (const auto& row : result) {
                sessions.push_back(rowToSession(row));
            }
            return sessions;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresSessionRepository] findByUserId() failed: " << e.what() << std::endl;
            return {};
        }
    }

    bool deleteById(const std::string& sessionId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                "DELETE FROM sessions WHERE session_id = $1",
                sessionId
            );
            
            txn.commit();
            return result.affected_rows() > 0;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresSessionRepository] deleteById() failed: " << e.what() << std::endl;
            return false;
        }
    }

    bool deleteByUserId(const std::string& userId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            
            auto result = txn.exec_params(
                "DELETE FROM sessions WHERE user_id = $1",
                userId
            );
            
            txn.commit();
            return result.affected_rows() > 0;
            
        } catch (const std::exception& e) {
            std::cerr << "[PostgresSessionRepository] deleteByUserId() failed: " << e.what() << std::endl;
            return false;
        }
    }

    void deleteExpired() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            pqxx::work txn(*connection_);
            txn.exec("DELETE FROM sessions WHERE expires_at < NOW()");
            txn.commit();
        } catch (const std::exception& e) {
            std::cerr << "[PostgresSessionRepository] deleteExpired() failed: " << e.what() << std::endl;
        }
    }

private:
    std::shared_ptr<DbSettings> settings_;
    std::unique_ptr<pqxx::connection> connection_;
    mutable std::mutex mutex_;

    domain::Session rowToSession(const pqxx::row& row) const {
        domain::Session session;
        session.sessionId = row["session_id"].as<std::string>();
        session.userId = row["user_id"].as<std::string>();
        session.jwtToken = row["jwt_token"].as<std::string>();
        
        auto expEpoch = row["exp_epoch"].as<int64_t>();
        session.expiresAt = std::chrono::system_clock::time_point(
            std::chrono::seconds(expEpoch)
        );
        
        return session;
    }
};

} // namespace auth::adapters::secondary
