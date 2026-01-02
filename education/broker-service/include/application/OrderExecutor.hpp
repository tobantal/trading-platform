// include/application/OrderExecutor.hpp
#pragma once

#include "ports/output/IBrokerGateway.hpp"
#include "ports/output/IEventPublisher.hpp"
#include "domain/OrderRequest.hpp"
#include "domain/OrderResult.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>
#include <chrono>

namespace broker::application {

/**
 * @brief Сервис исполнения ордеров
 * 
 * Отправляет ордера брокеру и публикует события в EventBus.
 * Использует строковый JSON-интерфейс для событий.
 */
class OrderExecutor {
public:
    OrderExecutor(
        std::shared_ptr<ports::output::IBrokerGateway> broker,
        std::shared_ptr<ports::output::IEventPublisher> eventPublisher
    ) : broker_(std::move(broker))
      , eventPublisher_(std::move(eventPublisher))
    {}

    domain::OrderResult placeOrder(const std::string& accountId, const domain::OrderRequest& request) {
        std::cout << "[OrderExecutor] Placing order for " << accountId << std::endl;
        
        auto result = broker_->placeOrder(accountId, request);
        
        if (result.isSuccess()) {
            // Публикуем событие order.filled в JSON формате
            nlohmann::json eventJson;
            eventJson["order_id"] = result.orderId;
            eventJson["account_id"] = accountId;
            eventJson["figi"] = request.figi;
            eventJson["executed_lots"] = result.executedLots;
            eventJson["executed_price"] = result.executedPrice.toDouble();
            eventJson["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            eventPublisher_->publish("order.filled", eventJson.dump());
            std::cout << "[OrderExecutor] Published order.filled event" << std::endl;
        } else {
            // Публикуем событие order.rejected
            nlohmann::json eventJson;
            eventJson["order_id"] = result.orderId;
            eventJson["account_id"] = accountId;
            eventJson["figi"] = request.figi;
            eventJson["reason"] = result.message;
            eventJson["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            eventPublisher_->publish("order.rejected", eventJson.dump());
            std::cout << "[OrderExecutor] Published order.rejected event" << std::endl;
        }
        
        return result;
    }

private:
    std::shared_ptr<ports::output::IBrokerGateway> broker_;
    std::shared_ptr<ports::output::IEventPublisher> eventPublisher_;
};

} // namespace broker::application
