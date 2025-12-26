#pragma once

#include <IHttpHandler.hpp>
#include "ports/input/IStrategyService.hpp"
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>
#include <regex>

namespace trading::adapters::primary {

/**
 * @brief HTTP Handler для торговых стратегий
 * 
 * Endpoints:
 * - POST /api/v1/strategies
 * - GET /api/v1/strategies
 * - GET /api/v1/strategies/{id}
 * - POST /api/v1/strategies/{id}/start
 * - POST /api/v1/strategies/{id}/stop
 * - DELETE /api/v1/strategies/{id}
 */
class StrategyHandler : public IHttpHandler
{
public:
    StrategyHandler(
        std::shared_ptr<ports::input::IStrategyService> strategyService,
        std::shared_ptr<ports::input::IAuthService> authService,
        std::shared_ptr<ports::input::IAccountService> accountService
    ) : strategyService_(std::move(strategyService))
      , authService_(std::move(authService))
      , accountService_(std::move(accountService))
    {
        std::cout << "[StrategyHandler] Created" << std::endl;
    }

    void handle(IRequest& req, IResponse& res) override
    {
        // Проверяем авторизацию
        auto userId = extractUserId(req);
        if (!userId) {
            res.setStatus(401);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Unauthorized"})");
            return;
        }

        auto account = accountService_->getActiveAccount(*userId);
        if (!account) {
            res.setStatus(400);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "No active account found"})");
            return;
        }

        std::string method = req.getMethod();
        std::string path = req.getPath();
        
        // Исправлено: точное определение endpoints с помощью регулярных выражений
        static const std::regex strategyIdRegex(R"(^/api/v1/strategies/([^/]+)$)");
        static const std::regex strategyStartRegex(R"(^/api/v1/strategies/([^/]+)/start$)");
        static const std::regex strategyStopRegex(R"(^/api/v1/strategies/([^/]+)/stop$)");
        
        std::smatch matches;
        
        if (method == "POST" && path == "/api/v1/strategies") {
            handleCreateStrategy(req, res, account->id);
        } else if (method == "GET" && path == "/api/v1/strategies") {
            handleGetStrategies(req, res, account->id);
        } else if (method == "GET" && std::regex_match(path, matches, strategyIdRegex)) {
            handleGetStrategy(req, res, account->id, matches[1].str());
        } else if (method == "POST" && std::regex_match(path, matches, strategyStartRegex)) {
            handleStartStrategy(req, res, account->id, matches[1].str());
        } else if (method == "POST" && std::regex_match(path, matches, strategyStopRegex)) {
            handleStopStrategy(req, res, account->id, matches[1].str());
        } else if (method == "DELETE" && std::regex_match(path, matches, strategyIdRegex)) {
            handleDeleteStrategy(req, res, account->id, matches[1].str());
        } else {
            res.setStatus(404);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Not found"})");
        }
    }

private:
    std::shared_ptr<ports::input::IStrategyService> strategyService_;
    std::shared_ptr<ports::input::IAuthService> authService_;
    std::shared_ptr<ports::input::IAccountService> accountService_;

    /**
     * @brief Обрабатывает запрос создания стратегии.
     */
    void handleCreateStrategy(IRequest& req, IResponse& res, const std::string& accountId)
    {
        try {
            auto body = nlohmann::json::parse(req.getBody());

            domain::StrategyRequest strategyReq;
            strategyReq.accountId = accountId;
            strategyReq.name = body.value("name", "");
            strategyReq.figi = body.value("figi", "");
            
            // Получаем тип стратегии из запроса, по умолчанию SMA_CROSSOVER
            std::string typeStr = body.value("type", "SMA_CROSSOVER");
            try {
                strategyReq.type = domain::strategyTypeFromString(typeStr);
            } catch (const std::invalid_argument&) {
                strategyReq.type = domain::StrategyType::SMA_CROSSOVER;
            }
            
            // Конфигурация стратегии
            if (strategyReq.type == domain::StrategyType::SMA_CROSSOVER) {
                nlohmann::json config;
                config["shortPeriod"] = body.value("short_period", 10);
                config["longPeriod"] = body.value("long_period", 30);
                config["quantity"] = body.value("quantity", 1);
                strategyReq.config = config.dump();
            } else {
                // Для других типов стратегий сохраняем конфиг как есть
                strategyReq.config = body.value("config", "{}");
            }

            // Валидация
            if (strategyReq.name.empty()) {
                res.setStatus(400);
                res.setHeader("Content-Type", "application/json");
                res.setBody(R"({"error": "Name is required"})");
                return;
            }
            if (strategyReq.figi.empty()) {
                res.setStatus(400);
                res.setHeader("Content-Type", "application/json");
                res.setBody(R"({"error": "FIGI is required"})");
                return;
            }

            auto strategy = strategyService_->createStrategy(strategyReq);
            
            res.setStatus(201);
            res.setHeader("Content-Type", "application/json");
            res.setBody(strategyToJson(strategy).dump());

        } catch (const nlohmann::json::exception& e) {
            res.setStatus(400);
            res.setHeader("Content-Type", "application/json");
            res.setBody(R"({"error": "Invalid JSON"})");
        } catch (const std::exception& e) {
            res.setStatus(500);
            res.setHeader("Content-Type", "application/json");
            nlohmann::json error;
            error["error"] = std::string("Internal server error: ") + e.what();
            res.setBody(error.dump());
        }
    }

    /**
     * @brief Обрабатывает запрос получения всех стратегий счёта.
     */
    void handleGetStrategies(IRequest& req, IResponse& res, const std::string& accountId)
    {
        try {
            auto strategies = strategyService_->getStrategiesByAccount(accountId);

            nlohmann::json response = nlohmann::json::array();
            for (const auto& strategy : strategies) {
                response.push_back(strategyToJson(strategy));
            }

            res.setStatus(200);
            res.setHeader("Content-Type", "application/json");
            res.setBody(response.dump());
        } catch (const std::exception& e) {
            res.setStatus(500);
            res.setHeader("Content-Type", "application/json");
            nlohmann::json error;
            error["error"] = std::string("Internal server error: ") + e.what();
            res.setBody(error.dump());
        }
    }

    /**
     * @brief Обрабатывает запрос получения стратегии по ID.
     */
    void handleGetStrategy(IRequest& req, IResponse& res, const std::string& accountId, const std::string& strategyId)
    {
        try {
            auto strategy = strategyService_->getStrategyById(strategyId);
            if (!strategy || strategy->accountId != accountId) {
                res.setStatus(404);
                res.setHeader("Content-Type", "application/json");
                res.setBody(R"({"error": "Strategy not found"})");
                return;
            }

            res.setStatus(200);
            res.setHeader("Content-Type", "application/json");
            res.setBody(strategyToJson(*strategy).dump());
        } catch (const std::exception& e) {
            res.setStatus(500);
            res.setHeader("Content-Type", "application/json");
            nlohmann::json error;
            error["error"] = std::string("Internal server error: ") + e.what();
            res.setBody(error.dump());
        }
    }

    /**
     * @brief Обрабатывает запрос запуска стратегии.
     */
    void handleStartStrategy(IRequest& req, IResponse& res, const std::string& accountId, const std::string& strategyId)
    {
        try {
            auto strategy = strategyService_->getStrategyById(strategyId);
            if (!strategy || strategy->accountId != accountId) {
                res.setStatus(404);
                res.setHeader("Content-Type", "application/json");
                res.setBody(R"({"error": "Strategy not found"})");
                return;
            }

            bool started = strategyService_->startStrategy(strategyId);
            
            if (started) {
                nlohmann::json response;
                response["message"] = "Strategy started";
                response["strategy_id"] = strategyId;
                res.setStatus(200);
                res.setHeader("Content-Type", "application/json");
                res.setBody(response.dump());
            } else {
                res.setStatus(400);
                res.setHeader("Content-Type", "application/json");
                res.setBody(R"({"error": "Cannot start strategy"})");
            }
        } catch (const std::exception& e) {
            res.setStatus(500);
            res.setHeader("Content-Type", "application/json");
            nlohmann::json error;
            error["error"] = std::string("Internal server error: ") + e.what();
            res.setBody(error.dump());
        }
    }

    /**
     * @brief Обрабатывает запрос остановки стратегии.
     */
    void handleStopStrategy(IRequest& req, IResponse& res, const std::string& accountId, const std::string& strategyId)
    {
        try {
            auto strategy = strategyService_->getStrategyById(strategyId);
            if (!strategy || strategy->accountId != accountId) {
                res.setStatus(404);
                res.setHeader("Content-Type", "application/json");
                res.setBody(R"({"error": "Strategy not found"})");
                return;
            }

            // Исправлено: интерфейс IStrategyService требует только strategyId
            bool stopped = strategyService_->stopStrategy(strategyId);
            
            if (stopped) {
                nlohmann::json response;
                response["message"] = "Strategy stopped";
                response["strategy_id"] = strategyId;
                res.setStatus(200);
                res.setHeader("Content-Type", "application/json");
                res.setBody(response.dump());
            } else {
                res.setStatus(400);
                res.setHeader("Content-Type", "application/json");
                res.setBody(R"({"error": "Cannot stop strategy"})");
            }
        } catch (const std::exception& e) {
            res.setStatus(500);
            res.setHeader("Content-Type", "application/json");
            nlohmann::json error;
            error["error"] = std::string("Internal server error: ") + e.what();
            res.setBody(error.dump());
        }
    }

    /**
     * @brief Обрабатывает запрос удаления стратегии.
     */
    void handleDeleteStrategy(IRequest& req, IResponse& res, const std::string& accountId, const std::string& strategyId)
    {
        try {
            auto strategy = strategyService_->getStrategyById(strategyId);
            if (!strategy || strategy->accountId != accountId) {
                res.setStatus(404);
                res.setHeader("Content-Type", "application/json");
                res.setBody(R"({"error": "Strategy not found"})");
                return;
            }

            bool deleted = strategyService_->deleteStrategy(strategyId);
            
            if (deleted) {
                nlohmann::json response;
                response["message"] = "Strategy deleted";
                res.setStatus(200);
                res.setHeader("Content-Type", "application/json");
                res.setBody(response.dump());
            } else {
                res.setStatus(400);
                res.setHeader("Content-Type", "application/json");
                res.setBody("{\"error\": \"Cannot delete strategy (stop it first)\"}");
            }
        } catch (const std::exception& e) {
            res.setStatus(500);
            res.setHeader("Content-Type", "application/json");
            nlohmann::json error;
            error["error"] = std::string("Internal server error: ") + e.what();
            res.setBody(error.dump());
        }
    }

    /**
     * @brief Преобразует объект Strategy в JSON.
     */
    nlohmann::json strategyToJson(const domain::Strategy& strategy)
    {
        nlohmann::json j;
        j["id"] = strategy.id;
        j["account_id"] = strategy.accountId;
        j["name"] = strategy.name;
        j["type"] = domain::toString(strategy.type);
        j["figi"] = strategy.figi;
        j["status"] = domain::toString(strategy.status);
        j["error_message"] = strategy.errorMessage;
        j["created_at"] = strategy.createdAt.toString();
        j["updated_at"] = strategy.updatedAt.toString();
        
        try {
            if (!strategy.config.empty()) {
                j["config"] = nlohmann::json::parse(strategy.config);
            } else {
                j["config"] = nlohmann::json::object();
            }
        } catch (const nlohmann::json::exception&) {
            j["config"] = strategy.config;
        }
        
        return j;
    }

    /**
     * @brief Извлекает userId из заголовков Authorization.
     */
    std::optional<std::string> extractUserId(IRequest& req)
    {
        auto headers = req.getHeaders();
        auto it = headers.find("Authorization");
        if (it == headers.end()) {
            return std::nullopt;
        }

        std::string auth = it->second;
        if (auth.find("Bearer ") != 0) {
            return std::nullopt;
        }

        std::string token = auth.substr(7);
        return authService_->getUserIdFromToken(token);
    }
};

} // namespace trading::adapters::primary