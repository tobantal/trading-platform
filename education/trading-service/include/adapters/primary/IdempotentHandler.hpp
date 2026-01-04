#pragma once

#include <IHttpHandler.hpp>
#include <IRequest.hpp>
#include <IResponse.hpp>
#include "ports/output/IIdempotencyRepository.hpp"
#include <memory>
#include <iostream>

namespace trading::adapters::primary
{

    // Wrapper для перехвата статуса и тела ответа
    class ResponseCapture : public IResponse
    {
    public:
        explicit ResponseCapture(IResponse &inner) : inner_(inner) {}

        void setStatus(int code) override
        {
            status_ = code;
            inner_.setStatus(code);
        }

        void setBody(const std::string &body) override
        {
            body_ = body;
            inner_.setBody(body);
        }

        void setHeader(const std::string &name, const std::string &value) override
        {
            inner_.setHeader(name, value);
        }

        int getStatus() const { return status_; }
        std::string getBody() const { return body_; }

    private:
        IResponse &inner_;
        int status_ = 0;
        std::string body_;
    };

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
            auto headers = req.getHeaders();
            std::string key;

            // Ищем X-Idempotency-Key в заголовках
            auto it = headers.find("X-Idempotency-Key");
            if (it != headers.end())
            {
                key = it->second;
            }

            // Только для POST и DELETE, если есть ключ
            if ((req.getMethod() == "POST" || req.getMethod() == "DELETE") && !key.empty())
            {
                std::cout << "[IdempotentHandler] Checking key: " << key << std::endl;

                // Проверяем - был ли уже такой запрос
                auto cached = repo_->find(key);
                if (cached)
                {
                    std::cout << "[IdempotentHandler] Cache HIT for key: " << key << std::endl;
                    res.setStatus(cached->status);
                    res.setBody(cached->body);
                    res.setHeader("Content-Type", "application/json");
                    res.setHeader("X-Idempotency-Key-Used", "true");
                    return;
                }

                std::cout << "[IdempotentHandler] Cache MISS for key: " << key << std::endl;

                // Выполняем оригинальный хэндлер с перехватом ответа
                ResponseCapture capture(res);
                inner_->handle(req, capture);

                // Сохраняем результат (только успешные)
                if (capture.getStatus() >= 200 && capture.getStatus() < 300)
                {
                    repo_->save(key, capture.getStatus(), capture.getBody());
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
