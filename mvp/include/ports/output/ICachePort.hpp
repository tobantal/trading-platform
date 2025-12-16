#pragma once

#include <string>
#include <optional>
#include <chrono>

namespace trading::ports::output {

/**
 * @brief Интерфейс кэша
 * 
 * Output Port для кэширования данных.
 * Используется для кэширования котировок, результатов поиска и т.д.
 * 
 * Реализации:
 * - LruCacheAdapter (MVP) - обёртка над cpp-cache
 * - RedisAdapter (future) - Redis
 */
class ICachePort {
public:
    virtual ~ICachePort() = default;

    /**
     * @brief Установить значение в кэш
     * 
     * @param key Ключ
     * @param value Значение (сериализованное в строку)
     * @param ttlSeconds Время жизни в секундах (0 = без ограничения)
     */
    virtual void set(
        const std::string& key,
        const std::string& value,
        int ttlSeconds = 0
    ) = 0;

    /**
     * @brief Получить значение из кэша
     * 
     * @param key Ключ
     * @return Значение или nullopt если не найдено или истекло
     */
    virtual std::optional<std::string> get(const std::string& key) = 0;

    /**
     * @brief Удалить значение из кэша
     * 
     * @param key Ключ
     * @return true если было удалено
     */
    virtual bool remove(const std::string& key) = 0;

    /**
     * @brief Проверить наличие ключа
     * 
     * @param key Ключ
     * @return true если существует и не истёк
     */
    virtual bool exists(const std::string& key) = 0;

    /**
     * @brief Очистить весь кэш
     */
    virtual void clear() = 0;

    /**
     * @brief Получить количество элементов в кэше
     * 
     * @return Количество элементов
     */
    virtual size_t size() const = 0;
};

} // namespace trading::ports::output