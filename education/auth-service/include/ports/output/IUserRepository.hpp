#pragma once

#include "domain/User.hpp"
#include <string>
#include <optional>

namespace auth::ports::output {

/**
 * @brief Интерфейс репозитория пользователей
 * 
 * Output Port для работы с хранилищем пользователей.
 */
class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    /**
     * @brief Сохранить пользователя
     * @param user Пользователь для сохранения
     * @return Сохранённый пользователь
     */
    virtual domain::User save(const domain::User& user) = 0;

    /**
     * @brief Найти пользователя по ID
     * @param userId ID пользователя
     * @return User или nullopt
     */
    virtual std::optional<domain::User> findById(const std::string& userId) = 0;

    /**
     * @brief Найти пользователя по username
     * @param username Имя пользователя
     * @return User или nullopt
     */
    virtual std::optional<domain::User> findByUsername(const std::string& username) = 0;

    /**
     * @brief Найти пользователя по email
     * @param email Email пользователя
     * @return User или nullopt
     */
    virtual std::optional<domain::User> findByEmail(const std::string& email) = 0;

    /**
     * @brief Проверить существование пользователя по username
     */
    virtual bool existsByUsername(const std::string& username) = 0;

    /**
     * @brief Проверить существование пользователя по email
     */
    virtual bool existsByEmail(const std::string& email) = 0;
};

} // namespace auth::ports::output
