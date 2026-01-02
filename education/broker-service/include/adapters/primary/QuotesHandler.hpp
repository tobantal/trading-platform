#pragma once

#include <IHttpHandler.hpp>
#include <IRequest.hpp>
#include <IResponse.hpp>
#include "ports/input/IQuoteService.hpp"
#include <nlohmann/json.hpp>
#include <memory>

namespace broker::adapters::primary {

/**
 * @brief HTTP handler for quotes endpoint
 */
class QuotesHandler : public IHttpHandler {
public:
    explicit QuotesHandler(std::shared_ptr<ports::input::IQuoteService> quoteService)
        : quoteService_(std::move(quoteService))
    {}

    void handle(IRequest& req, IResponse& res) override {
        std::string path = req.getPath();
        
        // Parse figi from query string
        std::string figi;
        auto pos = path.find("figi=");
        if (pos != std::string::npos) {
            figi = path.substr(pos + 5);
            auto ampPos = figi.find('&');
            if (ampPos != std::string::npos) {
                figi = figi.substr(0, ampPos);
            }
        }
        
        if (figi.empty()) {
            res.setStatus(400);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Missing figi parameter"})");
            return;
        }
        
        auto quote = quoteService_->getQuote(figi);
        if (!quote) {
            res.setStatus(404);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Quote not found"})");
            return;
        }
        
        nlohmann::json j;
        j["figi"] = quote->figi;
        j["bid_price"] = {
            {"units", quote->bidPrice.units},
            {"nano", quote->bidPrice.nano},
            {"currency", quote->bidPrice.currency}
        };
        j["ask_price"] = {
            {"units", quote->askPrice.units},
            {"nano", quote->askPrice.nano},
            {"currency", quote->askPrice.currency}
        };
        j["last_price"] = {
            {"units", quote->lastPrice.units},
            {"nano", quote->lastPrice.nano},
            {"currency", quote->lastPrice.currency}
        };
        j["updated_at"] = quote->updatedAt.toString();
        
        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(j.dump());
    }

private:
    std::shared_ptr<ports::input::IQuoteService> quoteService_;
};

} // namespace broker::adapters::primary
