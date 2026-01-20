#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IOrderService.hpp"
#include "ports/output/IAuthClient.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace trading::adapters::primary
{

    /**
     * @brief POST /api/v1/orders — создать ордер
     *
     * Требует Access Token (содержит accountId).
     */
    class CreateOrderHandler : public IHttpHandler
    {
    public:
        CreateOrderHandler(
            std::shared_ptr<ports::input::IOrderService> orderService,
            std::shared_ptr<ports::output::IAuthClient> authClient) : orderService_(std::move(orderService)), authClient_(std::move(authClient))
        {
            std::cout << "[CreateOrderHandler] Created" << std::endl;
        }

        void handle(IRequest &req, IResponse &res) override
        {
            if (req.getMethod() != "POST")
            {
                sendError(res, 405, "Method not allowed");
                return;
            }

            auto accountId = extractAccountId(req);
            if (!accountId)
            {
                sendError(res, 401, "Access token required. Use POST /api/v1/auth/select-account to get one.");
                return;
            }

            try
            {
                auto body = nlohmann::json::parse(req.getBody());

                domain::OrderRequest orderReq;
                orderReq.accountId = *accountId;
                orderReq.figi = body.value("figi", "");
                orderReq.quantity = body.value("quantity", 0);

                std::string direction = body.value("direction", "BUY");
                orderReq.direction = (direction == "SELL")
                                         ? domain::OrderDirection::SELL
                                         : domain::OrderDirection::BUY;

                std::string type = body.value("type", "MARKET");
                orderReq.type = (type == "LIMIT")
                                    ? domain::OrderType::LIMIT
                                    : domain::OrderType::MARKET;

                if (orderReq.type == domain::OrderType::LIMIT)
                {
                    orderReq.price = domain::Money::fromDouble(
                        body.value("price", 0.0),
                        body.value("currency", "RUB"));
                }

                if (orderReq.figi.empty())
                {
                    sendError(res, 400, "FIGI is required");
                    return;
                }
                if (orderReq.quantity <= 0)
                {
                    sendError(res, 400, "Quantity must be positive");
                    return;
                }

                auto result = orderService_->placeOrder(orderReq);

                nlohmann::json response;
                response["order_id"] = result.orderId;
                response["status"] = domain::toString(result.status);
                response["message"] = result.message;
                if (result.executedPrice.toDouble() > 0)
                {
                    response["executed_price"] = result.executedPrice.toDouble();
                }
                response["timestamp"] = result.timestamp.toString();

                int httpStatus = (result.status == domain::OrderStatus::REJECTED) ? 400 : 201;
                res.setResult(httpStatus, "application/json", response.dump());
            }
            catch (const nlohmann::json::exception &e)
            {
                sendError(res, 400, "Invalid JSON");
            }
            catch (const std::exception &e)
            {
                std::cerr << "[CreateOrderHandler] Error: " << e.what() << std::endl;
                sendError(res, 500, "Internal server error");
            }
        }

    private:
        std::shared_ptr<ports::input::IOrderService> orderService_;
        std::shared_ptr<ports::output::IAuthClient> authClient_;

        std::optional<std::string> extractAccountId(IRequest &req)
        {
            std::string token = req.getBearerToken().value_or("");
            return authClient_->getAccountIdFromToken(token);
        }

        void sendError(IResponse &res, int status, const std::string &message)
        {
            nlohmann::json error;
            error["error"] = message;
            res.setResult(status, "application/json", error.dump());
        }
    };

} // namespace trading::adapters::primary
