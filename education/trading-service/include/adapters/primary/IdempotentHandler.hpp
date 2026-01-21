#pragma once

#include <IHttpHandler.hpp>
#include <IRequest.hpp>
#include <IResponse.hpp>
#include "ports/output/IIdempotencyRepository.hpp"
#include <memory>
#include <iostream>

//FIXME: deprecated
// Использовать Middleware-классы вместо.
namespace trading::adapters::primary
{

    class IdempotentHandler : public IHttpHandler
    {
    public:
        IdempotentHandler(std::shared_ptr<IHttpHandler> inner,
                          std::shared_ptr<trading::ports::output::IIdempotencyRepository> repo)
            : inner_(inner), repo_(repo)
        {
            std::cout << "[IdempotentHandler] Created" << std::endl;
        }

        void handle(IRequest &req, IResponse &res) override
        {
            // Ищем X-Idempotency-Key
            std::string key = req.getHeader("X-Idempotency-Key").value_or("");

            // Только для POST и DELETE, если есть ключ
            if ((req.getMethod() == "POST" || req.getMethod() == "DELETE") && !key.empty())
            {
                std::cout << "[IdempotentHandler] Checking key: " << key << std::endl;

                // Проверяем - был ли уже такой запрос
                auto cached = repo_->find(key);
                if (cached)
                {
                    std::cout << "[IdempotentHandler] Cache HIT for key: " << key << std::endl;
                    res.setHeader("X-Idempotency-Key-Used", "true");
                    res.setResult(cached->status, "application/json", cached->body);
                    return;
                }

                std::cout << "[IdempotentHandler] Cache MISS for key: " << key << std::endl;
                inner_->handle(req, res);

                // Сохраняем результат (только успешные)
                if (res.getStatus() >= 200 && res.getStatus() < 300)
                {
                    repo_->save(key, res.getStatus(), res.getBody());
                }
            }
            else
            {
                // вызываем оригинальный хэндлер
                inner_->handle(req, res);
            }
        }

    private:
        std::shared_ptr<IHttpHandler> inner_;
        std::shared_ptr<ports::output::IIdempotencyRepository> repo_;
    };

} // namespace trading::adapters::primary
