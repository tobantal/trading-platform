#pragma once

#include "domain/Session.hpp"
#include <string>
#include <optional>

namespace auth::ports::output {

/**
 * @brief Интерфейс репозитория сессий
 */
class ISessionRepository {
public:
    virtual ~ISessionRepository() = default;

    virtual domain::Session save(const domain::Session& session) = 0;
    virtual std::optional<domain::Session> findById(const std::string& sessionId) = 0;
    virtual std::optional<domain::Session> findByToken(const std::string& jwtToken) = 0;
    virtual std::vector<domain::Session> findByUserId(const std::string& userId) = 0;
    virtual bool deleteById(const std::string& sessionId) = 0;
    virtual bool deleteByUserId(const std::string& userId) = 0;
    virtual void deleteExpired() = 0;
};

} // namespace auth::ports::output
