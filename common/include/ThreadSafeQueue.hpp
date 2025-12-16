#pragma once

#include "ICommand.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

/**
 * @file ThreadSafeQueue.hpp
 * @brief Потокобезопасная очередь команд
 * @details
 * Реализует thread-safe очередь с блокирующей операцией pop().
 * Использует std::mutex и std::condition_variable для синхронизации.
 *
 * Автор: Anton Tobolkin
 * Версия: 3.2 (refactored)
 */
class ThreadSafeQueue {
private:
    std::queue<std::shared_ptr<ICommand>> queue_;  ///< Внутренняя очередь
    mutable std::mutex mutex_;                     ///< Мьютекс для синхронизации
    std::condition_variable condVar_;              ///< Условная переменная для ожидания
    bool shutdown_ = false;                        ///< Флаг завершения работы очереди

public:
    ThreadSafeQueue();
    ~ThreadSafeQueue();

    /**
     * @brief Добавить команду в очередь
     * @param command Команда для добавления
     */
    void push(std::shared_ptr<ICommand> command);

    /**
     * @brief Извлечь команду из очереди (блокирующий вызов)
     * @return std::shared_ptr<ICommand> — команда, либо nullptr, если очередь закрыта
     */
    std::shared_ptr<ICommand> pop();

    /**
     * @brief Закрыть очередь и пробудить все ожидающие потоки
     */
    void shutdown();

    /**
     * @brief Проверить, закрыта ли очередь
     * @return true, если shutdown() уже был вызван
     */
    bool isShutdown() const;

    /**
     * @brief Проверить, пуста ли очередь
     * @return true, если очередь пуста
     */
    bool isEmpty() const;

    /**
     * @brief Получить количество элементов в очереди
     * @return Текущее количество элементов
     */
    size_t size() const;
};
