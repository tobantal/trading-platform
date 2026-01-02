#pragma once

#include <IHttpHandler.hpp>
#include <IRequest.hpp>
#include <IResponse.hpp>
#include "ports/input/IQuoteService.hpp"
#include <nlohmann/json.hpp>
#include <memory>

namespace broker::adapters::primary {

/**
 * @brief HTTP handler for instruments endpoint
 */
class InstrumentsHandler : public IHttpHandler {
public:
    explicit InstrumentsHandler(std::shared_ptr<ports::input::IQuoteService> quoteService)
        : quoteService_(std::move(quoteService))
    {}

    void handle(IRequest& req, IResponse& res) override {
        std::string path = req.getPath();
        std::string basePath = "/api/v1/instruments";
        
        if (path.length() > basePath.length() + 1) {
            // GET /api/v1/instruments/{figi}
            std::string figi = path.substr(basePath.length() + 1);
            auto instrument = quoteService_->getInstrument(figi);
            
            if (!instrument) {
                res.setStatus(404);
                res.setHeader("Content-Type", "application/json");
                res.setBody(R"({"error": "Instrument not found"})");
                return;
            }
            
            nlohmann::json j;
            j["figi"] = instrument->figi;
            j["ticker"] = instrument->ticker;
            j["name"] = instrument->name;
            j["currency"] = instrument->currency;
            j["lot"] = instrument->lot;
            
            res.setStatus(200);
            res.setHeader("Content-Type", "application/json");
            res.setBody(j.dump());
        } else {
            // GET /api/v1/instruments - list all
            auto instruments = quoteService_->getInstruments();
            
            nlohmann::json j = nlohmann::json::array();
            for (const auto& instr : instruments) {
                nlohmann::json item;
                item["figi"] = instr.figi;
                item["ticker"] = instr.ticker;
                item["name"] = instr.name;
                item["currency"] = instr.currency;
                item["lot"] = instr.lot;
                j.push_back(item);
            }
            
            res.setStatus(200);
            res.setHeader("Content-Type", "application/json");
            res.setBody(j.dump());
        }
    }

private:
    std::shared_ptr<ports::input::IQuoteService> quoteService_;
};

} // namespace broker::adapters::primary
