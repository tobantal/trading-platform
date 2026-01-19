// broker-service/include/adapters/primary/QuotesHandler.hpp
#pragma once

#include "ports/input/IQuoteService.hpp"
#include "IHttpHandler.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>
#include <sstream>
#include <vector>

namespace broker::adapters::primary {

/**
 * @brief HTTP handler for quotes endpoint
 * 
 * Поддерживает два формата:
 * - GET /api/v1/quotes?figi=BBG004730N88        (один инструмент)
 * - GET /api/v1/quotes?figis=BBG004730N88,BBG004730RP0  (несколько)
 */
class QuotesHandler : public IHttpHandler {
public:
    explicit QuotesHandler(std::shared_ptr<ports::input::IQuoteService> quoteService)
        : quoteService_(std::move(quoteService))
    {
        std::cout << "[QuotesHandler] Created" << std::endl;
    }

    void handle(IRequest& req, IResponse& res) override {
        // Поддержка обоих параметров: figi и figis
        std::vector<std::string> figis;
        
        auto itFigis = req.getQueryParam("figis").value_or("");
        auto itFigi = req.getQueryParam("figi").value_or("");
        
        if (!itFigis.empty()) {
            // Парсим список через запятую
            figis = parseFigis(itFigis);
        } else if (!itFigi.empty()) {
            // Один инструмент
            figis.push_back(itFigi);
        }
        
        if (figis.empty()) {
            res.setResult(400, "application/json", R"({"error": "Missing figi or figis parameter"})");
            return;
        }
        
        // Получаем котировки для всех инструментов
        nlohmann::json response;
        response["quotes"] = nlohmann::json::array();
        
        for (const auto& figi : figis) {
            auto quote = quoteService_->getQuote(figi);
            if (quote) {
                nlohmann::json q;
                q["figi"] = quote->figi;
                q["ticker"] = quote->ticker;
                q["bid_price"] = quote->bidPrice.toDouble();
                q["ask_price"] = quote->askPrice.toDouble();
                q["last_price"] = quote->lastPrice.toDouble();
                q["currency"] = quote->lastPrice.currency;
                response["quotes"].push_back(q);
            }
        }
        
        res.setResult(200, "application/json", response.dump());
    }

private:
    std::shared_ptr<ports::input::IQuoteService> quoteService_;
    
    static std::vector<std::string> parseFigis(const std::string& figisStr) {
        std::vector<std::string> result;
        std::stringstream ss(figisStr);
        std::string figi;
        
        while (std::getline(ss, figi, ',')) {
            if (!figi.empty()) {
                result.push_back(figi);
            }
        }
        
        return result;
    }
};

} // namespace broker::adapters::primary
