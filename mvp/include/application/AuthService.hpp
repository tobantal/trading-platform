#pragma once

#include "ports/input/IAuthService.hpp"
#include "ports/output/IJwtProvider.hpp"
#include "ports/output/IUserRepository.hpp"
#include "ports/output/IAccountRepository.hpp"
#include <memory>
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include <regex>

namespace trading::application {

/**
 * @brief Сервис аутентификации и авторизации
 * 
 * Реализует двухуровневую авторизацию:
 * 1. registerUser() → создание пользователя
 * 2. login() → session_token + список аккаунтов
 * 3. selectAccount() → access_token с accountId
 * 
 * Зависимости:
 * - IJwtProvider — создание/валидация JWT токенов
 * - IUserRepository — хранение пользователей
 * - IAccountRepository — хранение аккаунтов
 */
class AuthService : public ports::input::IAuthService {
public:
    /**
     * @brief Конструктор
     * 
     * @param jwtProvider Провайдер JWT токенов
     * @param userRepository Репозиторий пользователей
     * @param accountRepository Репозиторий аккаунтов
     */
    AuthService(
        std::shared_ptr<ports::output::IJwtProvider> jwtProvider,
        std::shared_ptr<ports::output::IUserRepository> userRepository,
        std::shared_ptr<ports::output::IAccountRepository> accountRepository
    ) : jwtProvider_(std::move(jwtProvider))
      , userRepository_(std::move(userRepository))
      , accountRepository_(std::move(accountRepository))
    {
        std::cout << "[AuthService] Created" << std::endl;
    }

    // ========================================================================
    // РЕГИСТРАЦИЯ
    // ========================================================================

    /**
     * @brief Зарегистрировать нового пользователя
     * 
     * Проверяет:
     * - Формат username (3-50 символов, [a-zA-Z0-9_])
     * - Уникальность username
     * - Длину пароля (минимум 6 символов)
     * 
     * Создаёт пользователя БЕЗ привязанных аккаунтов.
     */
    RegisterResult registerUser(
        const std::string& username,
        const std::string& password
    ) override {
        RegisterResult result;

        // Валидация username
        if (!isValidUsername(username)) {
            result.success = false;
            result.error = "Invalid username format. Use 3-50 characters: a-z, A-Z, 0-9, _";
            return result;
        }

        // Проверка уникальности
        auto existingUser = userRepository_->findByUsername(username);
        if (existingUser) {
            result.success = false;
            result.error = "Username already exists";
            return result;
        }

        // Валидация пароля
        if (password.length() < 6) {
            result.success = false;
            result.error = "Password too short. Minimum 6 characters";
            return result;
        }

        // Хэширование пароля
        // TODO: реализовать BCrypt хэширование
        std::string passwordHash = hashPassword(password);

        // Создание пользователя
        std::string userId = generateId("user");
        domain::User user(userId, username, passwordHash);
        userRepository_->save(user);

        result.success = true;
        result.userId = userId;

        std::cout << "[AuthService] User registered: " << username 
                  << " (id: " << userId << ")" << std::endl;

        return result;
    }

    // ========================================================================
    // ВХОД В СИСТЕМУ
    // ========================================================================

    /**
     * @brief Войти в систему
     * 
     * Проверяет существование пользователя и пароль.
     * НЕ создаёт пользователя — только проверяет.
     */
    LoginResult login(
        const std::string& username,
        const std::string& password
    ) override {
        LoginResult result;

        // Поиск пользователя
        auto userOpt = userRepository_->findByUsername(username);
        if (!userOpt) {
            result.success = false;
            result.error = "User not found";
            return result;
        }

        auto& user = *userOpt;

        // Проверка пароля
        // TODO: реализовать BCrypt сравнение
        if (!verifyPassword(password, user.passwordHash)) {
            result.success = false;
            result.error = "Invalid password";
            return result;
        }

        // Получение списка аккаунтов
        auto accounts = accountRepository_->findByUserId(user.id);

        // Создание session token
        std::string sessionToken = jwtProvider_->createSessionToken(user.id, user.username);

        result.success = true;
        result.sessionToken = sessionToken;
        result.tokenType = "Bearer";
        result.expiresIn = jwtProvider_->getSessionTokenLifetime();
        result.user = user;
        result.accounts = accounts;

        std::cout << "[AuthService] Login successful: " << username 
                  << ", accounts: " << accounts.size() << std::endl;

        return result;
    }

    // ========================================================================
    // ВЫБОР АККАУНТА
    // ========================================================================

    /**
     * @brief Выбрать аккаунт для работы
     * 
     * Проверяет:
     * - Валидность session token
     * - Существование аккаунта
     * - Принадлежность аккаунта пользователю
     * - Активность аккаунта
     */
    SelectAccountResult selectAccount(
        const std::string& sessionToken,
        const std::string& accountId
    ) override {
        SelectAccountResult result;

        // Валидация session token
        auto sessionClaims = jwtProvider_->extractSessionClaims(sessionToken);
        if (!sessionClaims) {
            result.success = false;
            result.error = "Invalid session token";
            return result;
        }

        // Поиск аккаунта
        auto accountOpt = accountRepository_->findById(accountId);
        if (!accountOpt) {
            result.success = false;
            result.error = "Account not found";
            return result;
        }

        auto& account = *accountOpt;

        // Проверка владельца
        if (account.userId != sessionClaims->userId) {
            result.success = false;
            result.error = "Account does not belong to user";
            return result;
        }

        // Проверка активности
        if (!account.active) {
            result.success = false;
            result.error = "Account is not active";
            return result;
        }

        // Создание access token
        std::string accessToken = jwtProvider_->createAccessToken(
            account.id,
            sessionClaims->userId,
            sessionClaims->username
        );

        result.success = true;
        result.accessToken = accessToken;
        result.tokenType = "Bearer";
        result.expiresIn = jwtProvider_->getAccessTokenLifetime();
        result.account = account;

        std::cout << "[AuthService] Account selected: " << accountId 
                  << " for user: " << sessionClaims->userId << std::endl;

        return result;
    }

    // ========================================================================
    // ВАЛИДАЦИЯ ТОКЕНОВ
    // ========================================================================

    /**
     * @brief Проверить валидность любого токена
     */
    ValidateResult validateToken(const std::string& token) override {
        ValidateResult result;

        if (!jwtProvider_->validateToken(token)) {
            result.valid = false;
            result.error = "Invalid or expired token";
            return result;
        }

        auto typeOpt = jwtProvider_->getTokenType(token);
        if (!typeOpt) {
            result.valid = false;
            result.error = "Unknown token type";
            return result;
        }

        result.valid = true;
        result.tokenType = domain::toString(*typeOpt);

        if (*typeOpt == domain::TokenType::SESSION) {
            auto claims = jwtProvider_->extractSessionClaims(token);
            if (claims) {
                result.userId = claims->userId;
                result.username = claims->username;
                result.remainingSeconds = claims->remainingSeconds();
            }
        } else {
            auto claims = jwtProvider_->extractAccessClaims(token);
            if (claims) {
                result.userId = claims->userId;
                result.username = claims->username;
                result.accountId = claims->accountId;
                result.remainingSeconds = claims->remainingSeconds();
            }
        }

        return result;
    }

    /**
     * @brief Проверить, является ли токен валидным access token
     */
    bool isValidAccessToken(const std::string& token) override {
        auto claims = jwtProvider_->extractAccessClaims(token);
        return claims.has_value() && claims->isValid();
    }

    /**
     * @brief Проверить, является ли токен валидным session token
     */
    bool isValidSessionToken(const std::string& token) override {
        auto claims = jwtProvider_->extractSessionClaims(token);
        return claims.has_value() && claims->isValid();
    }

    // ========================================================================
    // ОБНОВЛЕНИЕ ТОКЕНА
    // ========================================================================

    /**
     * @brief Обновить session token
     * 
     * Выдаёт новый токен, старый инвалидируется.
     */
    RefreshResult refreshSession(const std::string& sessionToken) override {
        RefreshResult result;

        auto claims = jwtProvider_->extractSessionClaims(sessionToken);
        if (!claims) {
            result.success = false;
            result.error = "Invalid session token";
            return result;
        }

        // Создаём новый токен
        result.sessionToken = jwtProvider_->createSessionToken(
            claims->userId,
            claims->username
        );
        result.expiresIn = jwtProvider_->getSessionTokenLifetime();
        result.success = true;

        // Инвалидируем старый
        jwtProvider_->invalidateToken(sessionToken);

        std::cout << "[AuthService] Session refreshed for user: " << claims->userId << std::endl;

        return result;
    }

    // ========================================================================
    // ИЗВЛЕЧЕНИЕ ДАННЫХ ИЗ ТОКЕНОВ
    // ========================================================================

    /**
     * @brief Извлечь accountId из access token
     */
    std::optional<std::string> getAccountIdFromToken(
        const std::string& accessToken
    ) override {
        auto claims = jwtProvider_->extractAccessClaims(accessToken);
        if (!claims || !claims->isValid()) {
            return std::nullopt;
        }
        return claims->accountId;
    }

    /**
     * @brief Извлечь userId из любого токена
     */
    std::optional<std::string> getUserIdFromToken(
        const std::string& token
    ) override {
        auto typeOpt = jwtProvider_->getTokenType(token);
        if (!typeOpt) return std::nullopt;

        if (*typeOpt == domain::TokenType::SESSION) {
            auto claims = jwtProvider_->extractSessionClaims(token);
            return claims ? std::optional(claims->userId) : std::nullopt;
        } else {
            auto claims = jwtProvider_->extractAccessClaims(token);
            return claims ? std::optional(claims->userId) : std::nullopt;
        }
    }

    /**
     * @brief Извлечь username из любого токена
     */
    std::optional<std::string> getUsernameFromToken(
        const std::string& token
    ) override {
        auto typeOpt = jwtProvider_->getTokenType(token);
        if (!typeOpt) return std::nullopt;

        if (*typeOpt == domain::TokenType::SESSION) {
            auto claims = jwtProvider_->extractSessionClaims(token);
            return claims ? std::optional(claims->username) : std::nullopt;
        } else {
            auto claims = jwtProvider_->extractAccessClaims(token);
            return claims ? std::optional(claims->username) : std::nullopt;
        }
    }

    // ========================================================================
    // ВЫХОД ИЗ СИСТЕМЫ
    // ========================================================================

    /**
     * @brief Выйти из системы (инвалидировать один токен)
     */
    void logout(const std::string& token) override {
        jwtProvider_->invalidateToken(token);
        std::cout << "[AuthService] Token invalidated" << std::endl;
    }

    /**
     * @brief Выйти из всех сессий пользователя
     */
    void logoutAll(const std::string& userId) override {
        jwtProvider_->invalidateAllUserTokens(userId);
        std::cout << "[AuthService] All tokens invalidated for user: " << userId << std::endl;
    }

private:
    std::shared_ptr<ports::output::IJwtProvider> jwtProvider_;
    std::shared_ptr<ports::output::IUserRepository> userRepository_;
    std::shared_ptr<ports::output::IAccountRepository> accountRepository_;

    /**
     * @brief Проверить формат username
     * 
     * Допустимые символы: a-z, A-Z, 0-9, _
     * Длина: 3-50 символов
     */
    static bool isValidUsername(const std::string& username) {
        if (username.length() < 3 || username.length() > 50) {
            return false;
        }
        static const std::regex pattern("^[a-zA-Z0-9_]+$");
        return std::regex_match(username, pattern);
    }

    /**
     * @brief Хэшировать пароль
     * 
     * TODO: реализовать BCrypt хэширование
     * Сейчас просто сохраняем как есть (небезопасно!)
     */
    static std::string hashPassword(const std::string& password) {
        // TODO: BCrypt
        // Временная заглушка — просто возвращаем пароль
        // WARNING: НЕ использовать в production!
        return password;
    }

    /**
     * @brief Проверить пароль
     * 
     * TODO: реализовать BCrypt сравнение
     */
    static bool verifyPassword(const std::string& password, const std::string& hash) {
        // TODO: BCrypt verify
        // Временная заглушка — простое сравнение
        // WARNING: НЕ использовать в production!
        return password == hash;
    }

    /**
     * @brief Генерация уникального ID
     * 
     * @param prefix Префикс (например, "user", "acc")
     * @return ID в формате "prefix-xxxxxxxx"
     */
    static std::string generateId(const std::string& prefix) {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint32_t> dist;

        std::ostringstream ss;
        ss << prefix << "-" << std::hex << std::setfill('0') << std::setw(8) << dist(gen);
        return ss.str();
    }
};

} // namespace trading::application
