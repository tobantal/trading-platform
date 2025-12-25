#pragma once

#include "ports/output/ICachePort.hpp"
#include <cache/Cache.hpp>
#include <cache/eviction/LRUPolicy.hpp>
#include <cache/concurrency/ThreadSafeCache.hpp>
#include <cache/expiration/GlobalTTL.hpp>
#include <memory>
#include <chrono>

namespace trading::adapters::secondary
{

    /**
     * @brief Адаптер кэширования на основе cpp-cache библиотеки
     *
     * Реализует ICachePort используя LRU кэш с опциональным TTL.
     * Thread-safe благодаря ThreadSafeCache wrapper.
     *
     * Используется для:
     * - Кэширования котировок
     * - Кэширования сессий
     * - Кэширования результатов запросов
     */
    class LruCacheAdapter : public ports::output::ICachePort
    {
    public:
        /**
         * @brief Конструктор
         *
         * @param capacity Максимальное количество элементов
         * @param ttlSeconds TTL в секундах (0 = без TTL)
         */
        explicit LruCacheAdapter(size_t capacity = 10000, int ttlSeconds = 0)
            : capacity_(capacity), ttlSeconds_(ttlSeconds)
        {
            if (ttlSeconds > 0)
            {
                // С TTL
                auto innerCache = std::make_unique<CacheType>(
                    capacity,
                    std::make_unique<LRUPolicy<std::string>>(),
                    std::make_unique<GlobalTTL<std::string>>(
                        std::chrono::seconds(ttlSeconds)));
                cache_ = std::make_unique<ThreadSafeCacheType>(std::move(innerCache));
            }
            else
            {
                // Без TTL
                auto innerCache = std::make_unique<CacheType>(
                    capacity,
                    std::make_unique<LRUPolicy<std::string>>());
                cache_ = std::make_unique<ThreadSafeCacheType>(std::move(innerCache));
            }
        }

        /**
         * @brief Получить значение из кэша
         */
        std::optional<std::string> get(const std::string &key) override
        {
            return cache_->get(key);
        }

        /**
         * @brief Сохранить значение с индивидуальным TTL
         *
         * @note В текущей реализации TTL игнорируется, используется глобальный.
         *       Для per-key TTL нужно использовать PerKeyTTL policy.
         */
        void set(const std::string &key, const std::string &value, int ttlSeconds) override
        {
            // TODO: Для per-key TTL нужна другая конфигурация кэша
            cache_->put(key, value);
        }

        /**
         * @brief Удалить значение из кэша
         */
        bool remove(const std::string &key) override
        {
            return cache_->remove(key);
        }

        /**
         * @brief Проверить наличие ключа
         */
        bool exists(const std::string &key) override
        {
            return cache_->get(key).has_value();
        }

        /**
         * @brief Очистить весь кэш
         */
        void clear() override
        {
            cache_->clear();
        }

        /**
         * @brief Получить количество элементов
         */
        size_t size() const override
        {
            return cache_->size();
        }

        /**
         * @brief Получить ёмкость кэша
         */
        size_t capacity() const
        {
            return capacity_;
        }

        /**
         * @brief Получить TTL в секундах
         */
        int ttlSeconds() const
        {
            return ttlSeconds_;
        }

    private:
        using CacheType = Cache<std::string, std::string>;
        using ThreadSafeCacheType = ThreadSafeCache<std::string, std::string>;

        size_t capacity_;
        int ttlSeconds_;
        std::unique_ptr<ThreadSafeCacheType> cache_;
    };

} // namespace trading::adapters::secondary
