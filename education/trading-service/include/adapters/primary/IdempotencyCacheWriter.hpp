

// adapters/primary/IdempotencyCacheWriter.hpp
#pragma once
#include <IHttpHandler.hpp>
#include "ports/output/IIdempotencyRepository.hpp"
#include <nlohmann/json.hpp>

namespace trading::adapters::primary
{

    class IdempotencyCacheWriter : public IHttpHandler
    {
    public:
        explicit IdempotencyCacheWriter(std::shared_ptr<trading::ports::output::IIdempotencyRepository> repo)
            : repo_(std::move(repo)) {}

        void handle(IRequest &req, IResponse &res) override
        {
            if (req.getMethod() != "POST" && req.getMethod() != "DELETE")
                return;

            // Сначала устанавливаем статус из атрибута (ВСЕГДА для POST/DELETE)
            std::string httpStatusStr = req.getAttribute("httpStatus").value_or("");
            if (!httpStatusStr.empty())
            {
                try
                {
                    auto status = std::stoi(httpStatusStr);
                    res.setStatus(status);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "[IdempotencyCacheWriter] Error: incorrect httpStatus="
                              << httpStatusStr << " " << e.what() << std::endl;
                    sendError(res, 500, "Internal server error");
                    return;
                }
            }

            // Сохраняем в репозиторий только если есть X-Idempotency-Key
            auto key = req.getHeader("X-Idempotency-Key").value_or("");
            if (key.empty())
            {
                return; // Статус уже установлен, просто выходим
            }

            if (res.getStatus() >= 200 && res.getStatus() < 300)
            {
                // TODO: сделать сервис (в котором репозиторий + кэш)
                repo_->save(key, res.getStatus(), res.getBody());
            }
        }

    private:
        std::shared_ptr<trading::ports::output::IIdempotencyRepository> repo_;

        void sendError(IResponse &res, int status, const std::string &message)
        {
            nlohmann::json error;
            error["error"] = message;
            res.setResult(status, "application/json", error.dump());
        }
    };

} // namespace trading::adapters::primary
