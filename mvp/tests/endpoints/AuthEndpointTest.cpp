#include <gtest/gtest.h>

#include "adapters/primary/AuthHandler.hpp"
#include "application/AuthService.hpp"
#include "adapters/secondary/auth/FakeJwtAdapter.hpp"
#include "adapters/secondary/persistence/InMemoryUserRepository.hpp"
#include "adapters/secondary/persistence/InMemoryAccountRepository.hpp"

#include <SimpleRequest.hpp>
#include <SimpleResponse.hpp>

using namespace trading::adapters::primary;
using namespace trading::application;
using namespace trading::adapters::secondary;
using namespace trading::domain;

// ============================================================================
// Тестовый класс AuthHandlerTest
// ============================================================================

/**
 * @brief Тестовый класс для AuthHandler.
 * Создаёт реальные зависимости для полного тестирования.
 * Использует InMemory репозитории и FakeJwtAdapter.
 * Тестирует эндпоинты /api/v1/auth/login и /api/v1/auth/validate.
 * Покрывает сценарии успешного и неуспешного логина, валидации токена,
 * а также создание новых пользователей и аккаунтов.
 * Использует Google Test framework.
 */
class AuthHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Создаем реальные зависимости
        auto jwtProvider = std::make_shared<FakeJwtAdapter>();
        auto userRepository = std::make_shared<InMemoryUserRepository>();
        auto accountRepository = std::make_shared<InMemoryAccountRepository>();
        
        // Создаем сервис с реальными зависимостями
        auto authService = std::make_shared<AuthService>(
            jwtProvider, userRepository, accountRepository);
        
        // Создаем handler для тестирования
        authHandler = std::make_unique<AuthHandler>(authService);
        
        // Сохраняем зависимости для использования в тестах
        this->jwtProvider = jwtProvider;
        this->userRepository = userRepository;
        this->accountRepository = accountRepository;
    }
    
    void TearDown() override {
        authHandler.reset();
    }
    
    // Вспомогательный метод для парсинга JSON ответа
    nlohmann::json parseJsonResponse(SimpleResponse& res) {
        try {
            return nlohmann::json::parse(res.getBody());
        } catch (const nlohmann::json::exception& e) {
            ADD_FAILURE() << "Failed to parse JSON: " << e.what();
            return nlohmann::json();
        }
    }
    
    std::unique_ptr<AuthHandler> authHandler;
    std::shared_ptr<FakeJwtAdapter> jwtProvider;
    std::shared_ptr<InMemoryUserRepository> userRepository;
    std::shared_ptr<InMemoryAccountRepository> accountRepository;
};

// ============================================================================
// ТЕСТЫ
// ============================================================================

/**
 * @brief Тестирует успешный логин с валидным именем пользователя.
 */
TEST_F(AuthHandlerTest, Login_WithValidUsername_ReturnsToken) {
    // Arrange
    SimpleRequest req(
        "POST",
        "/api/v1/auth/login",
        R"({"username": "testuser"})",
        "127.0.0.1",
        8080
    );
    SimpleResponse res;
    
    // Act
    authHandler->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 200);
    auto headers = res.getHeaders();
    EXPECT_TRUE(headers.find("Content-Type") != headers.end());
    EXPECT_EQ(headers.at("Content-Type"), "application/json");
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("access_token"));
    EXPECT_TRUE(json["access_token"].is_string());
    EXPECT_GT(json["access_token"].get<std::string>().size(), 0);
    
    EXPECT_EQ(json["token_type"], "Bearer");
    EXPECT_EQ(json["expires_in"], 3600);
}

/**
 * @brief Тестирует логин с пустым именем пользователя.
 * Ожидается ошибка 400 Bad Request.
 */
TEST_F(AuthHandlerTest, Login_WithEmptyUsername_Returns400) {
    // Arrange
    SimpleRequest req(
        "POST",
        "/api/v1/auth/login",
        R"({"username": ""})",
        "127.0.0.1",
        8080
    );
    SimpleResponse res;
    
    // Act
    authHandler->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "Username is required");
}

/**
 * @brief Тестирует логин с некорректным JSON в теле запроса.
 * Ожидается ошибка 400 Bad Request.
 */
TEST_F(AuthHandlerTest, Login_WithInvalidJson_Returns400) {
    // Arrange
    SimpleRequest req(
        "POST",
        "/api/v1/auth/login",
        R"({"username": "testuser" invalid json)",
        "127.0.0.1",
        8080
    );
    SimpleResponse res;
    
    // Act
    authHandler->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "Invalid JSON");
}

/**
 * @brief Тестирует создание нового пользователя и аккаунта при логине с новым именем пользователя.
 */
TEST_F(AuthHandlerTest, Login_CreatesUserAndAccount_WhenUserDoesNotExist) {
    // Arrange
    std::string newUsername = "newuser_" + std::to_string(time(nullptr));
    
    SimpleRequest req(
        "POST",
        "/api/v1/auth/login",
        "{\"username\": \"" + newUsername + "\"}",
        "127.0.0.1",
        8080
    );
    SimpleResponse res;
    
    // Act - Первый логин создает пользователя
    authHandler->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJsonResponse(res);
    std::string token = json["access_token"];
    EXPECT_FALSE(token.empty());
    
    // Проверяем, что пользователь создан
    auto user = userRepository->findByUsername(newUsername);
    EXPECT_TRUE(user.has_value());
    EXPECT_EQ(user->username, newUsername);
    
    // Проверяем, что счет создан
    auto accounts = accountRepository->findByUserId(user->id);
    EXPECT_FALSE(accounts.empty());
    
    // Проверяем, что счет - sandbox
    EXPECT_EQ(accounts[0].type, AccountType::SANDBOX);
}

/**
 * @brief Тестирует валидацию с валидным токеном.
 */
TEST_F(AuthHandlerTest, ValidateToken_WithValidToken_ReturnsValidTrue) {
    // Arrange
    // Сначала логинимся, чтобы получить токен
    SimpleRequest loginReq(
        "POST",
        "/api/v1/auth/login",
        R"({"username": "testuser2"})",
        "127.0.0.1",
        8080
    );
    SimpleResponse loginRes;
    
    authHandler->handle(loginReq, loginRes);
    auto loginJson = parseJsonResponse(loginRes);
    std::string validToken = loginJson["access_token"];
    
    // Теперь тестируем валидацию
    SimpleRequest validateReq(
        "POST",
        "/api/v1/auth/validate",
        "{\"token\": \"" + validToken + "\"}",
        "127.0.0.1",
        8080
    );
    SimpleResponse validateRes;
    
    // Act
    authHandler->handle(validateReq, validateRes);
    
    // Assert
    EXPECT_EQ(validateRes.getStatus(), 200);
    
    auto json = parseJsonResponse(validateRes);
    EXPECT_TRUE(json.contains("valid"));
    EXPECT_TRUE(json["valid"].get<bool>());
    
    EXPECT_TRUE(json.contains("user_id"));
    EXPECT_TRUE(json.contains("username"));
    EXPECT_EQ(json["username"], "testuser2");
}

/**
 * @brief Тестирует валидацию с невалидным токеном.
 */
TEST_F(AuthHandlerTest, ValidateToken_WithInvalidToken_ReturnsValidFalse) {
    // Arrange
    SimpleRequest req(
        "POST",
        "/api/v1/auth/validate",
        R"({"token": "invalid.token.here"})",
        "127.0.0.1",
        8080
    );
    SimpleResponse res;
    
    // Act
    authHandler->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 200);
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("valid"));
    EXPECT_FALSE(json["valid"].get<bool>());
}

/**
 * @brief Тестирует валидацию с пустым токеном.
 * Ожидается ошибка 400 Bad Request.
 */
TEST_F(AuthHandlerTest, ValidateToken_WithEmptyToken_Returns400) {
    // Arrange
    SimpleRequest req(
        "POST",
        "/api/v1/auth/validate",
        R"({"token": ""})",
        "127.0.0.1",
        8080
    );
    SimpleResponse res;
    
    // Act
    authHandler->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 400);
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "Token is required");
}

/**
 * @brief Тестирует обращение к неизвестному эндпоинту.
 * Ожидается ошибка 404 Not Found.
 */
TEST_F(AuthHandlerTest, UnknownEndpoint_Returns404) {
    // Arrange
    SimpleRequest req(
        "GET",
        "/api/v1/auth/unknown",
        "",
        "127.0.0.1",
        8080
    );
    SimpleResponse res;
    
    // Act
    authHandler->handle(req, res);
    
    // Assert
    EXPECT_EQ(res.getStatus(), 404);
    
    auto json = parseJsonResponse(res);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "Not found");
}

/**
 * @brief Тестирует множественные логины одного и того же пользователя.
 * Ожидается, что при повторном логине будет выдан новый токен,
 * но user_id останется тем же.
 */
TEST_F(AuthHandlerTest, MultipleLogins_SameUser_ReturnsSameUserId) {
    // Arrange
    std::string username = "multiuser";
    
    // Первый логин
    {
        SimpleRequest req1(
            "POST",
            "/api/v1/auth/login",
            "{\"username\": \"" + username + "\"}",
            "127.0.0.1",
            8080
        );
        SimpleResponse res1;
        
        authHandler->handle(req1, res1);
        auto json1 = parseJsonResponse(res1);
        std::string token1 = json1["access_token"];
        
        // Валидируем первый токен
        SimpleRequest validateReq(
            "POST",
            "/api/v1/auth/validate",
            "{\"token\": \"" + token1 + "\"}",
            "127.0.0.1",
            8080
        );
        SimpleResponse validateRes;
        
        authHandler->handle(validateReq, validateRes);
        auto validateJson = parseJsonResponse(validateRes);
        std::string userId1 = validateJson["user_id"];
        
        // Второй логин того же пользователя
        SimpleRequest req2(
            "POST",
            "/api/v1/auth/login",
            "{\"username\": \"" + username + "\"}",
            "127.0.0.1",
            8080
        );
        SimpleResponse res2;
        
        authHandler->handle(req2, res2);
        auto json2 = parseJsonResponse(res2);
        std::string token2 = json2["access_token"];
        
        // Проверяем, что токены разные (новый выпущен)
        EXPECT_NE(token1, token2);
        
        // Валидируем второй токен
        SimpleRequest validateReq2(
            "POST",
            "/api/v1/auth/validate",
            "{\"token\": \"" + token2 + "\"}",
            "127.0.0.1",
            8080
        );
        SimpleResponse validateRes2;
        
        authHandler->handle(validateReq2, validateRes2);
        auto validateJson2 = parseJsonResponse(validateRes2);
        std::string userId2 = validateJson2["user_id"];
        
        // Проверяем, что user_id одинаковый (тот же пользователь)
        EXPECT_EQ(userId1, userId2);
    }
}
