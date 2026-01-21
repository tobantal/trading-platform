// adapters/primary/IdempotencyCacheReader.hpp
#pragma once
#include <IHttpHandler.hpp>
#include "ports/output/IIdempotencyRepository.hpp"

namespace trading::adapters::primary
{

    class IdempotencyCacheReader : public IHttpHandler
    {
    public:
        explicit IdempotencyCacheReader(std::shared_ptr<trading::ports::output::IIdempotencyRepository> repo)
            : repo_(std::move(repo)) {}

        void handle(IRequest &req, IResponse &res) override
        {
            if (req.getMethod() != "POST" && req.getMethod() != "DELETE")
                return;

            auto key = req.getHeader("X-Idempotency-Key");
            if (!key || key->empty())
                return;

            if (auto cached = repo_->find(*key))
            {
                res.setHeader("X-Idempotency-Key-Used", "true");
                res.setResult(cached->status, "application/json", cached->body);
                // status != 0, chain остановится
            }
            // иначе status == 0, chain продолжится
        }

    private:
        std::shared_ptr<trading::ports::output::IIdempotencyRepository> repo_;
    };

} // namespace trading::adapters::primary
