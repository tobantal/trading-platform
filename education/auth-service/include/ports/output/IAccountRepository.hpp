#pragma once

#include "domain/Account.hpp"
#include <string>
#include <optional>
#include <vector>

namespace auth::ports::output {

/**
 * @brief Интерфейс репозитория аккаунтов
 */
class IAccountRepository {
public:
    virtual ~IAccountRepository() = default;

    virtual domain::Account save(const domain::Account& account) = 0;
    virtual std::optional<domain::Account> findById(const std::string& accountId) = 0;
    virtual std::vector<domain::Account> findByUserId(const std::string& userId) = 0;
    virtual bool deleteById(const std::string& accountId) = 0;
};

} // namespace auth::ports::output
