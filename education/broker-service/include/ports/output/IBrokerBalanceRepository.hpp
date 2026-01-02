// include/ports/output/IBrokerBalanceRepository.hpp
#pragma once

#include "domain/BrokerBalance.hpp"
#include <optional>
#include <string>
#include <cstdint>

namespace broker::ports::output {

/**
 * @brief Интерфейс репозитория баланса брокера
 * 
 * Поддерживает атомарные операции резервирования для Saga:
 * - reserve: зарезервировать средства при создании ордера
 * - commitReserved: списать резерв при исполнении
 * - releaseReserved: вернуть резерв при отмене/отклонении
 * 
 * @example
 * ```cpp
 * // При создании ордера
 * if (!balanceRepo->reserve(accountId, cost)) {
 *     return OrderResult::rejected("insufficient_funds");
 * }
 * 
 * // При исполнении
 * balanceRepo->commitReserved(accountId, cost);
 * 
 * // При отклонении/отмене
 * balanceRepo->releaseReserved(accountId, cost);
 * ```
 */
class IBrokerBalanceRepository {
public:
    virtual ~IBrokerBalanceRepository() = default;
    
    /**
     * @brief Найти баланс по ID аккаунта
     */
    virtual std::optional<domain::BrokerBalance> findByAccountId(const std::string& accountId) = 0;
    
    /**
     * @brief Сохранить новый баланс
     */
    virtual void save(const domain::BrokerBalance& balance) = 0;
    
    /**
     * @brief Обновить существующий баланс
     */
    virtual void update(const domain::BrokerBalance& balance) = 0;
    
    // =========================================================================
    // Атомарные операции резервирования
    // =========================================================================
    
    /**
     * @brief Зарезервировать средства
     * 
     * Атомарно: available -= amount, reserved += amount
     * 
     * @param accountId ID аккаунта
     * @param amount Сумма в копейках
     * @return true если успешно, false если недостаточно средств
     */
    virtual bool reserve(const std::string& accountId, int64_t amount) = 0;
    
    /**
     * @brief Подтвердить резерв (при исполнении ордера)
     * 
     * Атомарно: reserved -= amount
     * 
     * @param accountId ID аккаунта
     * @param amount Сумма в копейках
     */
    virtual void commitReserved(const std::string& accountId, int64_t amount) = 0;
    
    /**
     * @brief Освободить резерв (при отмене/отклонении)
     * 
     * Атомарно: reserved -= amount, available += amount
     * 
     * @param accountId ID аккаунта
     * @param amount Сумма в копейках
     */
    virtual void releaseReserved(const std::string& accountId, int64_t amount) = 0;
};

} // namespace broker::ports::output
