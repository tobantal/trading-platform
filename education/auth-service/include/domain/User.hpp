#pragma once

#include <string>
#include <chrono>

namespace auth::domain {

/**
 * @brief Пользователь системы
 * 
 * Базовая сущность для аутентификации.
 * Пользователь может иметь несколько привязанных брокерских аккаунтов.
 */
struct User {
    std::string userId;         ///< UUID пользователя (формат: "user-xxxxxxxx")
    std::string username;       ///< Уникальное имя пользователя
    std::string email;          ///< Email для восстановления пароля
    std::string passwordHash;   ///< Хэш пароля (bcrypt)
    std::chrono::system_clock::time_point createdAt;  ///< Дата регистрации

    User() = default;

    User(const std::string& userId, 
         const std::string& username,
         const std::string& email,
         const std::string& passwordHash = "")
        : userId(userId)
        , username(username)
        , email(email)
        , passwordHash(passwordHash)
        , createdAt(std::chrono::system_clock::now()) 
    {}
};

} // namespace auth::domain
