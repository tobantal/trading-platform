// trading-service/include/ports/input/IOrderService.hpp
#pragma once

#include "domain/OrderRequest.hpp"
#include "domain/OrderResult.hpp"
#include "domain/Order.hpp"
#include <vector>
#include <optional>
#include <string>

namespace trading::ports::input {

/**
 * @brief Интерфейс сервиса ордеров
 */
class IOrderService {
public:
    virtual ~IOrderService() = default;

    /**
     * @brief Разместить ордер
     */
    virtual domain::OrderResult placeOrder(const domain::OrderRequest& request) = 0;

    /**
     * @brief Отменить ордер
     */
    virtual bool cancelOrder(const std::string& accountId, const std::string& orderId) = 0;

    /**
     * @brief Получить ордер по ID
     * @param accountId ID аккаунта (нужен для запроса к broker)
     * @param orderId ID ордера
     */
    virtual std::optional<domain::Order> getOrderById(
        const std::string& accountId, 
        const std::string& orderId) = 0;

    /**
     * @brief Получить все ордера аккаунта
     */
    virtual std::vector<domain::Order> getAllOrders(const std::string& accountId) = 0;
};

} // namespace trading::ports::input
