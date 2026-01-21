// adapters/primary/ChainHandler.hpp
#pragma once
#include <IHttpHandler.hpp>
#include <memory>
#include <vector>

// TODO: перенести в библиотеку cpp-http-server-lib после успешного внедрения.
namespace serverlib
{

    class ChainHandler : public IHttpHandler
    {
    public:
        template <typename... Handlers>
        explicit ChainHandler(Handlers &&...handlers)
        {
            (handlers_.push_back(std::forward<Handlers>(handlers)), ...);
        }

        void handle(IRequest &req, IResponse &res) override
        {
            for (auto &h : handlers_)
            {
                h->handle(req, res);
                if (res.getStatus() != 0)
                    return;
            }
        }

    private:
        std::vector<std::shared_ptr<IHttpHandler>> handlers_;
    };

} // namespace serverlib
