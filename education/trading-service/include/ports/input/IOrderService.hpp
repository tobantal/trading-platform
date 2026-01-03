#pragma once

#include "domain/OrderRequest.hpp"
#include "domain/OrderResult.hpp"
#include "domain/Order.hpp"
#include <vector>
#include <string>
#include <optional>

namespace trading::ports::input {

/**
 * @brief Интерфейс сервиса управления ордерами
 */
class IOrderService {
public:
    virtual ~IOrderService() = default;

    /**
     * @brief Создать ордер
     */
    virtual domain::OrderResult placeOrder(const domain::OrderRequest& request) = 0;

    /**
     * @brief Отменить ордер
     */
    virtual bool cancelOrder(const std::string& accountId, const std::string& orderId) = 0;

    /**
     * @brief Получить ордер по ID
     */
    virtual std::optional<domain::Order> getOrderById(const std::string& orderId) = 0;

    /**
     * @brief Получить все ордера аккаунта
     */
    virtual std::vector<domain::Order> getAllOrders(const std::string& accountId) = 0;
};

} // namespace trading::ports::input
