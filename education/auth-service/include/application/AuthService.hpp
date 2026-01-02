#pragma once

#include "ports/input/IAuthService.hpp"
#include "ports/output/IUserRepository.hpp"
#include "ports/output/ISessionRepository.hpp"
#include "ports/output/IJwtProvider.hpp"
#include "adapters/secondary/AuthSettings.hpp"
#include <memory>
#include <iostream>

namespace auth::application {

/**
 * @brief Сервис аутентификации
 */
class AuthService : public ports::input::IAuthService {
public:
    AuthService(
        std::shared_ptr<adapters::secondary::AuthSettings> settings,
        std::shared_ptr<ports::output::IUserRepository> userRepo,
        std::shared_ptr<ports::output::ISessionRepository> sessionRepo,
        std::shared_ptr<ports::output::IJwtProvider> jwtProvider
    ) : settings_(std::move(settings))
      , userRepo_(std::move(userRepo))
      , sessionRepo_(std::move(sessionRepo))
      , jwtProvider_(std::move(jwtProvider))
    {
        std::cout << "[AuthService] Created" << std::endl;
    }

    ports::input::RegisterResult registerUser(
        const std::string& username,
        const std::string& email,
        const std::string& password
    ) override {
        // Проверяем уникальность
        if (userRepo_->existsByUsername(username)) {
            return {false, "", "Username already exists"};
        }
        if (userRepo_->existsByEmail(email)) {
            return {false, "", "Email already exists"};
        }

        // Генерируем ID и хэшируем пароль
        std::string userId = "user-" + generateUuid();
        std::string passwordHash = hashPassword(password);

        // Сохраняем пользователя
        domain::User user(userId, username, email, passwordHash);
        userRepo_->save(user);

        return {true, userId, "User registered successfully"};
    }

    ports::input::LoginResult login(
        const std::string& username,
        const std::string& password
    ) override {
        auto userOpt = userRepo_->findByUsername(username);
        if (!userOpt) {
            return {false, "", "", "User not found"};
        }

        if (!verifyPassword(password, userOpt->passwordHash)) {
            return {false, "", "", "Invalid password"};
        }

        // Создаём session token
        std::string token = jwtProvider_->createSessionToken(
            userOpt->userId, 
            settings_->getSessionLifetimeSeconds()
        );

        // Сохраняем сессию
        std::string sessionId = "sess-" + generateUuid();
        domain::Session session(
            sessionId, 
            userOpt->userId, 
            token, 
            std::chrono::seconds(settings_->getSessionLifetimeSeconds())
        );
        sessionRepo_->save(session);

        return {true, token, userOpt->userId, "Login successful"};
    }

    bool logout(const std::string& sessionToken) override {
        auto sessionOpt = sessionRepo_->findByToken(sessionToken);
        if (!sessionOpt) {
            return false;
        }

        jwtProvider_->invalidateToken(sessionToken);
        sessionRepo_->deleteById(sessionOpt->sessionId);
        return true;
    }

    ports::input::ValidateResult validateSessionToken(const std::string& token) override {
        if (!jwtProvider_->isValidToken(token)) {
            return {false, "", "", "Invalid or expired token"};
        }

        auto userId = jwtProvider_->getUserId(token);
        if (!userId) {
            return {false, "", "", "Cannot extract user from token"};
        }

        return {true, *userId, "", "Valid"};
    }

    ports::input::ValidateResult validateAccessToken(const std::string& token) override {
        if (!jwtProvider_->isValidToken(token)) {
            return {false, "", "", "Invalid or expired token"};
        }

        auto userId = jwtProvider_->getUserId(token);
        auto accountId = jwtProvider_->getAccountId(token);
        
        if (!userId || !accountId) {
            return {false, "", "", "Invalid access token format"};
        }

        return {true, *userId, *accountId, "Valid"};
    }

    std::optional<std::string> createAccessToken(
        const std::string& sessionToken,
        const std::string& accountId
    ) override {
        auto result = validateSessionToken(sessionToken);
        if (!result.valid) {
            return std::nullopt;
        }

        return jwtProvider_->createAccessToken(
            result.userId,
            accountId,
            3600  // 1 час
        );
    }

private:
    std::shared_ptr<adapters::secondary::AuthSettings> settings_;
    std::shared_ptr<ports::output::IUserRepository> userRepo_;
    std::shared_ptr<ports::output::ISessionRepository> sessionRepo_;
    std::shared_ptr<ports::output::IJwtProvider> jwtProvider_;

    std::string generateUuid() {
        // Простая генерация UUID для демо
        static int counter = 0;
        return std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) 
               + "-" + std::to_string(++counter);
    }

    std::string hashPassword(const std::string& password) {
        // TODO: Реализовать bcrypt
        // Для демо - простой хэш
        return "hash:" + password;
    }

    bool verifyPassword(const std::string& password, const std::string& hash) {
        // TODO: Реализовать bcrypt verify
        return hash == "hash:" + password;
    }
};

} // namespace auth::application
