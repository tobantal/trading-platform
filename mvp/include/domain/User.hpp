#pragma once

#include "Timestamp.hpp"
#include <string>

namespace trading::domain {

/**
 * @brief Пользователь системы
 * 
 * Базовая сущность для аутентификации.
 * Пользователь может иметь несколько привязанных брокерских аккаунтов.
 */
struct User {
    std::string id;             ///< UUID пользователя (формат: "user-xxxxxxxx")
    std::string username;       ///< Уникальное имя пользователя
    std::string passwordHash;   ///< Хэш пароля (TODO: реализовать BCrypt)
    Timestamp createdAt;        ///< Дата регистрации

    User() = default;

    /**
     * @brief Конструктор для создания пользователя
     * 
     * @param id UUID пользователя
     * @param username Уникальное имя
     * @param passwordHash Хэш пароля
     */
    User(const std::string& id, 
         const std::string& username,
         const std::string& passwordHash = "")
        : id(id)
        , username(username)
        , passwordHash(passwordHash)
        , createdAt(Timestamp::now()) 
    {}
};

} // namespace trading::domain
