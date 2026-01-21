// adapters/primary/ChainHandler.hpp
#pragma once
#include <IHttpHandler.hpp>
#include <memory>
#include <vector>
#include <nlohmann/json.hpp>

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

            // если в конце статус все равно = 0, то это ошибка бизнес логики
            if (res.getStatus() == 0) {
                std::cerr << "[ChainHandler] Error: " << "middleware chain finished, but httpStatus is zero." << std::endl;
                sendError(res, 500, "Internal server error");
            }
        }

    private:
        std::vector<std::shared_ptr<IHttpHandler>> handlers_;

         void sendError(IResponse &res, int status, const std::string &message)
        {
            nlohmann::json error;
            error["error"] = message;
            res.setResult(status, "application/json", error.dump());
        }
    };

} // namespace serverlib
