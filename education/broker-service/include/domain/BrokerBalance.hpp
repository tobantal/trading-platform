// include/domain/BrokerBalance.hpp
#pragma once

#include <string>
#include <cstdint>

namespace broker::domain {

/**
 * @brief Баланс счёта с поддержкой резервирования
 * 
 * Реализует паттерн резервирования средств для Saga:
 * - available: доступно для торговли
 * - reserved: зарезервировано под активные ордера
 * 
 * Суммы хранятся в копейках (int64_t) для точности.
 * 
 * Жизненный цикл резервирования:
 * 
 * 1. Создание ордера BUY:
 *    ```
 *    cost = quantity × lot_size × price
 *    if (balance.canReserve(cost)) {
 *        balance.reserve(cost);  // available -= cost, reserved += cost
 *        order.status = PENDING;
 *    } else {
 *        order.status = REJECTED;  // insufficient funds
 *    }
 *    ```
 * 
 * 2. Ордер исполнен (FILLED):
 *    ```
 *    balance.commitReserved(cost);  // reserved -= cost
 *    // Деньги ушли на покупку акций, не возвращаются в available
 *    ```
 * 
 * 3. Ордер отклонён (REJECTED) или отменён (CANCELLED):
 *    ```
 *    balance.releaseReserved(cost);  // reserved -= cost, available += cost
 *    // Деньги вернулись
 *    ```
 * 
 * 4. Частичное исполнение (PARTIALLY_FILLED):
 *    ```
 *    filled_cost = filled_qty × lot_size × price;
 *    balance.commitReserved(filled_cost);  // исполненная часть
 *    // Остаток остаётся в reserved, ждёт исполнения
 *    ```
 * 
 * @example
 * ```
 * Начало: available=1_000_000, reserved=0
 * 
 * 1. BUY 10 лотов SBER @ 265₽, cost = 10×10×265 = 26_500
 *    reserve(26_500) → available=973_500, reserved=26_500
 * 
 * 2a. FILLED: commitReserved(26_500) → reserved=0
 * 2b. REJECTED: releaseReserved(26_500) → available=1_000_000, reserved=0
 * ```
 */
struct BrokerBalance {
    std::string accountId;      ///< Идентификатор счёта
    int64_t available = 0;      ///< Доступно для торговли (копейки)
    int64_t reserved = 0;       ///< Зарезервировано под ордера (копейки)
    std::string currency = "RUB";  ///< Валюта
    
    /**
     * @brief Общий баланс = available + reserved
     */
    int64_t total() const { 
        return available + reserved; 
    }
    
    /**
     * @brief Проверить, можно ли зарезервировать сумму
     * @param amount Сумма в копейках
     * @return true если available >= amount
     */
    bool canReserve(int64_t amount) const { 
        return available >= amount; 
    }
    
    /**
     * @brief Зарезервировать средства (при создании ордера)
     * 
     * Переводит средства из available в reserved.
     * 
     * @param amount Сумма в копейках
     * @return true если успешно, false если недостаточно средств
     */
    bool reserve(int64_t amount) {
        if (!canReserve(amount)) return false;
        available -= amount;
        reserved += amount;
        return true;
    }
    
    /**
     * @brief Подтвердить резерв (при исполнении ордера)
     * 
     * Списывает средства из reserved (деньги ушли на покупку).
     * 
     * @param amount Сумма в копейках
     */
    void commitReserved(int64_t amount) {
        reserved -= amount;
        // Деньги ушли на покупку акций - не возвращаются в available
    }
    
    /**
     * @brief Освободить резерв (при отмене/отклонении ордера)
     * 
     * Возвращает средства из reserved в available.
     * 
     * @param amount Сумма в копейках
     */
    void releaseReserved(int64_t amount) {
        reserved -= amount;
        available += amount;  // Деньги вернулись
    }
    
    /**
     * @brief Получить available в рублях (double)
     */
    double availableRub() const {
        return static_cast<double>(available) / 100.0;
    }
    
    /**
     * @brief Получить reserved в рублях (double)
     */
    double reservedRub() const {
        return static_cast<double>(reserved) / 100.0;
    }
    
    /**
     * @brief Получить total в рублях (double)
     */
    double totalRub() const {
        return static_cast<double>(total()) / 100.0;
    }
    
    /**
     * @brief Создать баланс из суммы в рублях
     */
    static BrokerBalance fromRub(const std::string& accountId, double rubAmount) {
        BrokerBalance b;
        b.accountId = accountId;
        b.available = static_cast<int64_t>(rubAmount * 100.0);
        b.reserved = 0;
        b.currency = "RUB";
        return b;
    }
};

} // namespace broker::domain
