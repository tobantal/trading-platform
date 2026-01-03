#pragma once

#include "ports/output/IAuthClient.hpp"
#include <map>
#include <string>

namespace trading::tests {

/**
 * @brief Mock реализация IAuthClient для тестов
 */
class MockAuthClient : public ports::output::IAuthClient {
public:
    // Настройка ответов
    void addValidToken(const std::string& token, const std::string& userId, const std::string& accountId) {
        validTokens_[token] = {userId, accountId};
    }

    void clearTokens() {
        validTokens_.clear();
    }

    // Счётчики вызовов
    int validateCallCount() const { return validateCallCount_; }
    void resetCallCount() { validateCallCount_ = 0; }

    // IAuthClient implementation
    ports::output::TokenValidationResult validateAccessToken(const std::string& token) override {
        ++validateCallCount_;
        
        ports::output::TokenValidationResult result;
        auto it = validTokens_.find(token);
        
        if (it != validTokens_.end()) {
            result.valid = true;
            result.userId = it->second.first;
            result.accountId = it->second.second;
            result.message = "OK";
        } else {
            result.valid = false;
            result.message = "Invalid token";
        }
        
        return result;
    }

    std::optional<std::string> getAccountIdFromToken(const std::string& token) override {
        auto result = validateAccessToken(token);
        if (result.valid) {
            return result.accountId;
        }
        return std::nullopt;
    }

private:
    std::map<std::string, std::pair<std::string, std::string>> validTokens_;  // token -> (userId, accountId)
    mutable int validateCallCount_ = 0;
};

} // namespace trading::tests
