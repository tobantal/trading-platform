#pragma once

#include "domain/OrderRequest.hpp"
#include "domain/OrderResult.hpp"
#include "domain/Order.hpp"
#include "domain/Timestamp.hpp"
#include <string>
#include <vector>
#include <optional>

namespace trading::ports::input {

/**
 * @brief Интерфейс сервиса управления ордерами
 * 
 * Input Port для создания, отмены и отслеживания торговых ордеров.
 */
class IOrderService {
public:
    virtual ~IOrderService() = default;

    /**
     * @brief Создать и разместить ордер
     * 
     * @param request Параметры ордера
     * @return Результат размещения
     * 
     * @note Публикует OrderCreatedEvent при успехе
     */
    virtual domain::OrderResult placeOrder(const domain::OrderRequest& request) = 0;

    /**
     * @brief Отменить ордер
     * 
     * @param accountId ID счёта
     * @param orderId ID ордера
     * @return true если отмена успешна
     * 
     * @note Можно отменить только ордер в статусе PENDING
     * @note Публикует OrderCancelledEvent при успехе
     */
    virtual bool cancelOrder(const std::string& accountId, const std::string& orderId) = 0;

    /**
     * @brief Получить ордер по ID
     * 
     * @param orderId ID ордера
     * @return Order или nullopt
     */
    virtual std::optional<domain::Order> getOrderById(const std::string& orderId) = 0;

    /**
     * @brief Получить список активных ордеров счёта
     * 
     * @param accountId ID счёта
     * @return Список ордеров в статусе PENDING
     */
    virtual std::vector<domain::Order> getActiveOrders(const std::string& accountId) = 0;

    /**
     * @brief Получить историю ордеров за период
     * 
     * @param accountId ID счёта
     * @param from Начало периода
     * @param to Конец периода
     * @return Список ордеров
     */
    virtual std::vector<domain::Order> getOrderHistory(
        const std::string& accountId,
        const domain::Timestamp& from,
        const domain::Timestamp& to
    ) = 0;

    /**
     * @brief Получить все ордера счёта
     * 
     * @param accountId ID счёта
     * @return Список всех ордеров
     */
    virtual std::vector<domain::Order> getAllOrders(const std::string& accountId) = 0;
};

} // namespace trading::ports::input