#pragma once

#include "ports/output/ISessionRepository.hpp"
#include <unordered_map>
#include <mutex>
#include <algorithm>

namespace auth::tests::mocks {

/**
 * @brief In-Memory реализация репозитория сессий для unit-тестов
 */
class InMemorySessionRepository : public ports::output::ISessionRepository {
public:
    domain::Session save(const domain::Session& session) override {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_[session.sessionId] = session;
        tokenIndex_[session.jwtToken] = session.sessionId;
        return session;
    }

    std::optional<domain::Session> findById(const std::string& sessionId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) return std::nullopt;
        return it->second;
    }

    std::optional<domain::Session> findByToken(const std::string& jwtToken) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tokenIndex_.find(jwtToken);
        if (it == tokenIndex_.end()) return std::nullopt;
        return sessions_[it->second];
    }

    std::vector<domain::Session> findByUserId(const std::string& userId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<domain::Session> result;
        for (const auto& [id, session] : sessions_) {
            if (session.userId == userId) {
                result.push_back(session);
            }
        }
        return result;
    }

    bool deleteById(const std::string& sessionId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) return false;
        
        tokenIndex_.erase(it->second.jwtToken);
        sessions_.erase(it);
        return true;
    }

    bool deleteByUserId(const std::string& userId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> toDelete;
        for (const auto& [id, session] : sessions_) {
            if (session.userId == userId) {
                toDelete.push_back(id);
            }
        }
        for (const auto& id : toDelete) {
            tokenIndex_.erase(sessions_[id].jwtToken);
            sessions_.erase(id);
        }
        return !toDelete.empty();
    }

    void deleteExpired() override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        std::vector<std::string> toDelete;
        for (const auto& [id, session] : sessions_) {
            if (session.expiresAt < now) {
                toDelete.push_back(id);
            }
        }
        for (const auto& id : toDelete) {
            tokenIndex_.erase(sessions_[id].jwtToken);
            sessions_.erase(id);
        }
    }

    // Test helpers
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.clear();
        tokenIndex_.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return sessions_.size();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, domain::Session> sessions_;
    std::unordered_map<std::string, std::string> tokenIndex_;  // token -> sessionId
};

} // namespace auth::tests::mocks
