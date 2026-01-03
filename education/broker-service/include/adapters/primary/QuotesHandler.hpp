#pragma once

#include <IHttpHandler.hpp>
#include <IRequest.hpp>
#include <IResponse.hpp>
#include "ports/input/IQuoteService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

namespace broker::adapters::primary {

/**
 * @brief HTTP handler for quotes endpoint
 * 
 * GET /api/v1/quotes?figi=BBG004730N88
 */
class QuotesHandler : public IHttpHandler {
public:
    explicit QuotesHandler(std::shared_ptr<ports::input::IQuoteService> quoteService)
        : quoteService_(std::move(quoteService))
    {
        std::cout << "[QuotesHandler] Created" << std::endl;
    }

    void handle(IRequest& req, IResponse& res) override {
        // Получаем figi из query параметров (НЕ из path!)
        auto params = req.getParams();
        auto it = params.find("figi");
        
        if (it == params.end() || it->second.empty()) {
            res.setStatus(400);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Missing figi parameter"})");
            return;
        }
        
        std::string figi = it->second;
        
        auto quote = quoteService_->getQuote(figi);
        if (!quote) {
            res.setStatus(404);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Quote not found for figi: )" + figi + R"("})");
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
