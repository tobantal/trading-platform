#pragma once

#include "ports/input/IStrategyService.hpp"
#include "ports/input/IAuthService.hpp"
#include "ports/input/IAccountService.hpp"
#include <http_server/IRequestHandler.hpp>
#include <nlohmann/json.hpp>
#include <memory>

namespace trading::adapters::primary {

/**
 * @brief REST контроллер торговых стратегий
 * 
 * Endpoints:
 * - POST /api/v1/strategies - создать стратегию
 * - GET /api/v1/strategies - список стратегий
 * - GET /api/v1/strategies/{id} - получить стратегию
 * - POST /api/v1/strategies/{id}/start - запустить
 * - POST /api/v1/strategies/{id}/stop - остановить
 * - DELETE /api/v1/strategies/{id} - удалить
 */
class StrategyController : public microservice::IRequestHandler {
public:
    StrategyController(
        std::shared_ptr<ports::input::IStrategyService> strategyService,
        std::shared_ptr<ports::input::IAuthService> authService,
        std::shared_ptr<ports::input::IAccountService> accountService
    ) : strategyService_(std::move(strategyService))
      , authService_(std::move(authService))
      , accountService_(std::move(accountService))
    {}

    microservice::Response handle(const microservice::Request& request) override {
        // Проверяем авторизацию
        auto userId = extractUserId(request);
        if (!userId) {
            return unauthorized();
        }

        auto account = accountService_->getActiveAccount(*userId);
        if (!account) {
            return badRequest("No active account found");
        }

        // POST /api/v1/strategies
        if (request.method == "POST" && request.path == "/api/v1/strategies") {
            return handleCreateStrategy(request, account->id);
        }
        
        // GET /api/v1/strategies
        if (request.method == "GET" && request.path == "/api/v1/strategies") {
            return handleGetStrategies(account->id);
        }
        
        // POST /api/v1/strategies/{id}/start
        if (request.method == "POST" && request.path.find("/start") != std::string::npos) {
            return handleStartStrategy(request, account->id);
        }
        
        // POST /api/v1/strategies/{id}/stop
        if (request.method == "POST" && request.path.find("/stop") != std::string::npos) {
            return handleStopStrategy(request, account->id);
        }
        
        // GET /api/v1/strategies/{id}
        if (request.method == "GET" && request.path.find("/api/v1/strategies/") == 0) {
            return handleGetStrategy(request, account->id);
        }
        
        // DELETE /api/v1/strategies/{id}
        if (request.method == "DELETE" && request.path.find("/api/v1/strategies/") == 0) {
            return handleDeleteStrategy(request, account->id);
        }

        return notFound();
    }

private:
    std::shared_ptr<ports::input::IStrategyService> strategyService_;
    std::shared_ptr<ports::input::IAuthService> authService_;
    std::shared_ptr<ports::input::IAccountService> accountService_;

    microservice::Response handleCreateStrategy(const microservice::Request& request, const std::string& accountId) {
        try {
            auto body = nlohmann::json::parse(request.body);

            domain::StrategyRequest strategyReq;
            strategyReq.accountId = accountId;
            strategyReq.name = body.value("name", "");
            strategyReq.figi = body.value("figi", "");
            
            std::string typeStr = body.value("type", "SMA_CROSSOVER");
            strategyReq.type = domain::StrategyType::SMA_CROSSOVER; // MVP: только SMA
            
            // Конфигурация SMA
            nlohmann::json config;
            config["shortPeriod"] = body.value("short_period", 10);
            config["longPeriod"] = body.value("long_period", 30);
            config["quantity"] = body.value("quantity", 1);
            strategyReq.config = config.dump();

            // Валидация
            if (strategyReq.name.empty()) {
                return badRequest("Name is required");
            }
            if (strategyReq.figi.empty()) {
                return badRequest("FIGI is required");
            }

            auto strategy = strategyService_->createStrategy(strategyReq);
            return jsonResponse(201, strategyToJson(strategy));

        } catch (const nlohmann::json::exception& e) {
            return badRequest("Invalid JSON: " + std::string(e.what()));
        }
    }

    microservice::Response handleGetStrategies(const std::string& accountId) {
        auto strategies = strategyService_->getStrategiesByAccount(accountId);

        nlohmann::json response = nlohmann::json::array();
        for (const auto& strategy : strategies) {
            response.push_back(strategyToJson(strategy));
        }

        return jsonResponse(200, response);
    }

    microservice::Response handleGetStrategy(const microservice::Request& request, const std::string& accountId) {
        std::string strategyId = extractStrategyId(request.path);
        
        auto strategy = strategyService_->getStrategyById(strategyId);
        if (!strategy || strategy->accountId != accountId) {
            return notFound("Strategy not found");
        }

        return jsonResponse(200, strategyToJson(*strategy));
    }

    microservice::Response handleStartStrategy(const microservice::Request& request, const std::string& accountId) {
        std::string strategyId = extractStrategyId(request.path);
        
        auto strategy = strategyService_->getStrategyById(strategyId);
        if (!strategy || strategy->accountId != accountId) {
            return notFound("Strategy not found");
        }

        bool started = strategyService_->startStrategy(strategyId);
        
        if (started) {
            nlohmann::json response;
            response["message"] = "Strategy started";
            response["strategy_id"] = strategyId;
            return jsonResponse(200, response);
        } else {
            return badRequest("Cannot start strategy");
        }
    }

    microservice::Response handleStopStrategy(const microservice::Request& request, const std::string& accountId) {
        std::string strategyId = extractStrategyId(request.path);
        
        auto strategy = strategyService_->getStrategyById(strategyId);
        if (!strategy || strategy->accountId != accountId) {
            return notFound("Strategy not found");
        }

        bool stopped = strategyService_->stopStrategy(strategyId, "Stopped by user");
        
        if (stopped) {
            nlohmann::json response;
            response["message"] = "Strategy stopped";
            response["strategy_id"] = strategyId;
            return jsonResponse(200, response);
        } else {
            return badRequest("Cannot stop strategy");
        }
    }

    microservice::Response handleDeleteStrategy(const microservice::Request& request, const std::string& accountId) {
        std::string strategyId = extractStrategyId(request.path);
        
        auto strategy = strategyService_->getStrategyById(strategyId);
        if (!strategy || strategy->accountId != accountId) {
            return notFound("Strategy not found");
        }

        bool deleted = strategyService_->deleteStrategy(strategyId);
        
        if (deleted) {
            nlohmann::json response;
            response["message"] = "Strategy deleted";
            return jsonResponse(200, response);
        } else {
            return badRequest("Cannot delete strategy (stop it first)");
        }
    }

    std::string extractStrategyId(const std::string& path) {
        // /api/v1/strategies/{id} или /api/v1/strategies/{id}/start
        std::string prefix = "/api/v1/strategies/";
        size_t start = prefix.length();
        size_t end = path.find('/', start);
        return (end == std::string::npos) 
            ? path.substr(start) 
            : path.substr(start, end - start);
    }

    nlohmann::json strategyToJson(const domain::Strategy& strategy) {
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
        
        // Парсим конфиг
        try {
            j["config"] = nlohmann::json::parse(strategy.config);
        } catch (...) {
            j["config"] = strategy.config;
        }
        
        return j;
    }

    std::optional<std::string> extractUserId(const microservice::Request& request) {
        auto it = request.headers.find("Authorization");
        if (it == request.headers.end()) {
            return std::nullopt;
        }

        std::string auth = it->second;
        if (auth.find("Bearer ") != 0) {
            return std::nullopt;
        }

        std::string token = auth.substr(7);
        return authService_->getUserIdFromToken(token);
    }

    microservice::Response jsonResponse(int status, const nlohmann::json& body) {
        microservice::Response resp;
        resp.status = status;
        resp.headers["Content-Type"] = "application/json";
        resp.body = body.dump();
        return resp;
    }

    microservice::Response badRequest(const std::string& message) {
        nlohmann::json body;
        body["error"] = message;
        return jsonResponse(400, body);
    }

    microservice::Response unauthorized() {
        nlohmann::json body;
        body["error"] = "Unauthorized";
        return jsonResponse(401, body);
    }

    microservice::Response notFound(const std::string& message = "Not found") {
        nlohmann::json body;
        body["error"] = message;
        return jsonResponse(404, body);
    }
};

} // namespace trading::adapters::primary
