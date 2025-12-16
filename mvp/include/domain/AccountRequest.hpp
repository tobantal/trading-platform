#pragma once

#include "enums/AccountType.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Запрос на создание счёта
 */
struct AccountRequest {
    std::string name;           ///< Название счёта
    AccountType type;           ///< Тип счёта
    std::string accessToken;    ///< API токен Tinkoff
};

} // namespace trading::domain