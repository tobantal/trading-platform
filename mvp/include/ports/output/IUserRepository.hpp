#pragma once

#include "domain/User.hpp"
#include <string>
#include <optional>
#include <vector>

namespace trading::ports::output {

/**
 * @brief Интерфейс репозитория пользователей
 * 
 * Output Port для сохранения и загрузки пользователей из БД.
 */
class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    /**
     * @brief Сохранить пользователя
     * 
     * @param user Пользователь для сохранения
     * 
     * @note Если пользователь с таким ID существует - обновляет
     */
    virtual void save(const domain::User& user) = 0;

    /**
     * @brief Найти пользователя по ID
     * 
     * @param id UUID пользователя
     * @return User или nullopt
     */
    virtual std::optional<domain::User> findById(const std::string& id) = 0;

    /**
     * @brief Найти пользователя по username
     * 
     * @param username Имя пользователя
     * @return User или nullopt
     */
    virtual std::optional<domain::User> findByUsername(const std::string& username) = 0;

    /**
     * @brief Удалить пользователя
     * 
     * @param id UUID пользователя
     * @return true если удалён
     */
    virtual bool deleteById(const std::string& id) = 0;

    /**
     * @brief Проверить существование пользователя
     * 
     * @param username Имя пользователя
     * @return true если существует
     */
    virtual bool existsByUsername(const std::string& username) = 0;
};

} // namespace trading::ports::output