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
        auto params = req.getParams();
        
        // Поддержка обоих параметров: figi и figis
        std::vector<std::string> figis;
        
        auto itFigis = params.find("figis");
        auto itFigi = params.find("figi");
        
        if (itFigis != params.end() && !itFigis->second.empty()) {
            // Парсим список через запятую
            figis = parseFigis(itFigis->second);
        } else if (itFigi != params.end() && !itFigi->second.empty()) {
            // Один инструмент
            figis.push_back(itFigi->second);
        }
        
        if (figis.empty()) {
            res.setStatus(400);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Missing figi or figis parameter"})");
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
        
        res.setStatus(200);
        res.setHeader("Content-Type", "application/json");
        res.setBody(response.dump());
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
