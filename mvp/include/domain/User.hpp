#pragma once

#include "Timestamp.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Пользователь системы
 * 
 * Базовая сущность для авторизации.
 * В MVP упрощённая - только username, без пароля.
 */
struct User {
    std::string id;             ///< UUID пользователя
    std::string username;       ///< Уникальное имя пользователя
    Timestamp createdAt;        ///< Дата регистрации

    User() = default;

    User(const std::string& id, const std::string& username)
        : id(id), username(username), createdAt(Timestamp::now()) {}
};

} // namespace trading::domain