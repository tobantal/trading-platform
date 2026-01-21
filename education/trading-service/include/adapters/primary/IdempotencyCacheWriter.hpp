

// adapters/primary/IdempotencyCacheWriter.hpp
#pragma once
#include <IHttpHandler.hpp>
#include "ports/output/IIdempotencyRepository.hpp"

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

            auto key = req.getHeader("X-Idempotency-Key");
            if (!key || key->empty())
                return;

            if (res.getStatus() >= 200 && res.getStatus() < 300)
            {
                repo_->save(*key, res.getStatus(), res.getBody());
            }
        }

    private:
        std::shared_ptr<trading::ports::output::IIdempotencyRepository> repo_;
    };

} // namespace trading::adapters::primary
