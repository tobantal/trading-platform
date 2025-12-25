#pragma once

#include "ports/input/IAuthService.hpp"
#include "ports/output/IJwtProvider.hpp"
#include "ports/output/IUserRepository.hpp"
#include "ports/output/IAccountRepository.hpp"
#include "domain/User.hpp"
#include <memory>
#include <random>
#include <sstream>
#include <iomanip>

namespace trading::application {

/**
 * @brief Сервис аутентификации
 * 
 * Реализует IAuthService, координирует работу между:
 * - IJwtProvider (создание/валидация токенов)
 * - IUserRepository (хранение пользователей)
 * - IAccountRepository (создание sandbox счёта при первом логине)
 */
class AuthService : public ports::input::IAuthService {
public:
    AuthService(
        std::shared_ptr<ports::output::IJwtProvider> jwtProvider,
        std::shared_ptr<ports::output::IUserRepository> userRepository,
        std::shared_ptr<ports::output::IAccountRepository> accountRepository
    ) : jwtProvider_(std::move(jwtProvider))
      , userRepository_(std::move(userRepository))
      , accountRepository_(std::move(accountRepository))
      , rng_(std::random_device{}())
    {}

    /**
     * @brief Выполнить логин пользователя
     * 
     * Для MVP: если пользователь не существует, создаём его и sandbox счёт.
     */
    LoginResult login(const std::string& username) override {
        LoginResult result;
        result.tokenType = "Bearer";
        result.expiresIn = 3600; // 1 час

        if (username.empty()) {
            result.success = false;
            result.error = "Username cannot be empty";
            return result;
        }

        // Ищем или создаём пользователя
        auto existingUser = userRepository_->findByUsername(username);
        domain::User user;
        
        if (existingUser) {
            user = *existingUser;
        } else {
            // Создаём нового пользователя
            user = domain::User(generateUuid(), username);
            userRepository_->save(user);
            
            // Создаём sandbox счёт для нового пользователя
            domain::Account account(
                generateUuid(),
                user.id,
                "Sandbox " + username,
                domain::AccountType::SANDBOX,
                "",
                true
            );
            accountRepository_->save(account);
        }

        // Создаём JWT токен
        result.accessToken = jwtProvider_->createToken(user.id, user.username);
        result.success = true;

        return result;
    }

    /**
     * @brief Валидировать JWT токен
     */
    bool validateToken(const std::string& token) override {
        return jwtProvider_->validateToken(token);
    }

    /**
     * @brief Извлечь userId из токена
     */
    std::optional<std::string> getUserIdFromToken(const std::string& token) override {
        auto claims = jwtProvider_->extractClaims(token);
        if (!claims || claims->isExpired()) {
            return std::nullopt;
        }
        return claims->userId;
    }

    /**
     * @brief Извлечь username из токена
     */
    std::optional<std::string> getUsernameFromToken(const std::string& token) override {
        auto claims = jwtProvider_->extractClaims(token);
        if (!claims || claims->isExpired()) {
            return std::nullopt;
        }
        return claims->username;
    }

private:
    std::shared_ptr<ports::output::IJwtProvider> jwtProvider_;
    std::shared_ptr<ports::output::IUserRepository> userRepository_;
    std::shared_ptr<ports::output::IAccountRepository> accountRepository_;
    std::mt19937_64 rng_;

    std::string generateUuid() {
        std::uniform_int_distribution<uint64_t> dist;
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        ss << std::setw(8) << (dist(rng_) & 0xFFFFFFFF) << "-";
        ss << std::setw(4) << (dist(rng_) & 0xFFFF) << "-";
        ss << std::setw(4) << ((dist(rng_) & 0x0FFF) | 0x4000) << "-";
        ss << std::setw(4) << ((dist(rng_) & 0x3FFF) | 0x8000) << "-";
        ss << std::setw(12) << (dist(rng_) & 0xFFFFFFFFFFFF);
        return ss.str();
    }
};

} // namespace trading::application
