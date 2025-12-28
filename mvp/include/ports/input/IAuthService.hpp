#pragma once

#include "domain/Account.hpp"
#include "domain/User.hpp"
#include <string>
#include <optional>
#include <vector>

namespace trading::ports::input {

/**
 * @brief Интерфейс сервиса аутентификации и авторизации
 * 
 * Реализует двухуровневую авторизацию:
 * 
 * Уровень 1 - Session Token (24 часа):
 *   - Выдаётся после успешного login
 *   - Используется для: выбора аккаунта, refresh, logout, получения списка аккаунтов
 *   - НЕ используется для работы с торговым API
 * 
 * Уровень 2 - Access Token (1 час):
 *   - Выдаётся после выбора аккаунта (select-account)
 *   - Содержит accountId в payload
 *   - Используется для всех торговых операций (orders, portfolio, strategies)
 * 
 * Типичный флоу:
 *   1. registerUser() — создание пользователя (один раз)
 *   2. login() — получение session_token
 *   3. selectAccount() — получение access_token для конкретного аккаунта
 *   4. Работа с API используя access_token
 *   5. logout() — завершение сессии
 */
class IAuthService {
public:
    virtual ~IAuthService() = default;

    // ========================================================================
    // РЕЗУЛЬТАТЫ ОПЕРАЦИЙ
    // ========================================================================

    /**
     * @brief Результат регистрации пользователя
     */
    struct RegisterResult {
        bool success = false;           ///< Успешность операции
        std::string error;              ///< Сообщение об ошибке (если success=false)
        std::string userId;             ///< ID созданного пользователя (если success=true)
    };

    /**
     * @brief Результат входа в систему
     */
    struct LoginResult {
        bool success = false;           ///< Успешность операции
        std::string error;              ///< Сообщение об ошибке (если success=false)
        
        std::string sessionToken;       ///< JWT session token для управления сессией
        std::string tokenType = "Bearer"; ///< Тип токена (всегда "Bearer")
        int expiresIn = 0;              ///< Время жизни токена в секундах (86400 = 24ч)
        
        domain::User user;              ///< Данные пользователя
        std::vector<domain::Account> accounts; ///< Список привязанных аккаунтов
    };

    /**
     * @brief Результат выбора аккаунта
     */
    struct SelectAccountResult {
        bool success = false;           ///< Успешность операции
        std::string error;              ///< Сообщение об ошибке (если success=false)
        
        std::string accessToken;        ///< JWT access token для работы с API
        std::string tokenType = "Bearer"; ///< Тип токена (всегда "Bearer")
        int expiresIn = 0;              ///< Время жизни токена в секундах (3600 = 1ч)
        
        domain::Account account;        ///< Выбранный аккаунт
    };

    /**
     * @brief Результат валидации токена
     */
    struct ValidateResult {
        bool valid = false;             ///< Валиден ли токен
        std::string error;              ///< Причина невалидности (если valid=false)
        
        std::string tokenType;          ///< "session" или "access"
        std::string userId;             ///< ID пользователя
        std::string username;           ///< Имя пользователя
        std::optional<std::string> accountId; ///< ID аккаунта (только для access token)
        int remainingSeconds = 0;       ///< Оставшееся время жизни в секундах
    };

    /**
     * @brief Результат обновления session token
     */
    struct RefreshResult {
        bool success = false;           ///< Успешность операции
        std::string error;              ///< Сообщение об ошибке (если success=false)
        
        std::string sessionToken;       ///< Новый session token
        int expiresIn = 0;              ///< Время жизни в секундах
    };

    // ========================================================================
    // РЕГИСТРАЦИЯ
    // ========================================================================

    /**
     * @brief Зарегистрировать нового пользователя
     * 
     * Создаёт пользователя в системе. После регистрации пользователь
     * должен привязать хотя бы один брокерский аккаунт через IAccountService.
     * 
     * @param username Уникальное имя пользователя (3-50 символов, [a-zA-Z0-9_])
     * @param password Пароль (минимум 6 символов)
     * 
     * @return RegisterResult:
     *   - success=true, userId заполнен — пользователь создан
     *   - success=false, error="Username already exists" — имя занято
     *   - success=false, error="Invalid username format" — неверный формат имени
     *   - success=false, error="Password too short" — пароль < 6 символов
     * 
     * @note Пароль хэшируется перед сохранением (TODO: реализовать BCrypt)
     * @note Пользователь создаётся БЕЗ привязанных аккаунтов
     */
    virtual RegisterResult registerUser(
        const std::string& username,
        const std::string& password
    ) = 0;

    // ========================================================================
    // ВХОД В СИСТЕМУ
    // ========================================================================

    /**
     * @brief Войти в систему
     * 
     * Проверяет учётные данные и выдаёт session token.
     * Session token используется для управления сессией, но НЕ для торговых операций.
     * 
     * @param username Имя пользователя
     * @param password Пароль
     * 
     * @return LoginResult:
     *   - success=true — sessionToken, user, accounts заполнены
     *   - success=false, error="User not found" — пользователь не существует
     *   - success=false, error="Invalid password" — неверный пароль
     * 
     * @note НЕ создаёт пользователя. Для создания использовать registerUser()
     * @note Возвращает список привязанных аккаунтов (может быть пустым)
     */
    virtual LoginResult login(
        const std::string& username,
        const std::string& password
    ) = 0;

    // ========================================================================
    // ВЫБОР АККАУНТА
    // ========================================================================

    /**
     * @brief Выбрать аккаунт для работы
     * 
     * Выдаёт access token, привязанный к конкретному брокерскому аккаунту.
     * Access token содержит accountId и используется для всех торговых операций.
     * 
     * @param sessionToken Валидный session token (из login)
     * @param accountId ID аккаунта для выбора
     * 
     * @return SelectAccountResult:
     *   - success=true — accessToken, account заполнены
     *   - success=false, error="Invalid session token" — токен невалиден/истёк
     *   - success=false, error="Account not found" — аккаунт не существует
     *   - success=false, error="Account does not belong to user" — чужой аккаунт
     *   - success=false, error="Account is not active" — аккаунт деактивирован
     * 
     * @note Требует session token, НЕ access token
     * @note Можно вызывать несколько раз для переключения между аккаунтами
     */
    virtual SelectAccountResult selectAccount(
        const std::string& sessionToken,
        const std::string& accountId
    ) = 0;

    // ========================================================================
    // ВАЛИДАЦИЯ ТОКЕНОВ
    // ========================================================================

    /**
     * @brief Проверить валидность любого токена
     * 
     * Определяет тип токена (session/access) и проверяет его валидность.
     * 
     * @param token JWT токен (session или access)
     * 
     * @return ValidateResult:
     *   - valid=true — токен валиден, поля заполнены
     *   - valid=false — токен невалиден/истёк, error содержит причину
     */
    virtual ValidateResult validateToken(const std::string& token) = 0;

    /**
     * @brief Проверить, является ли токен валидным access token
     * 
     * @param token JWT токен
     * @return true если это валидный, не истёкший access token
     * 
     * @note Используется хэндлерами для проверки авторизации перед торговыми операциями
     */
    virtual bool isValidAccessToken(const std::string& token) = 0;

    /**
     * @brief Проверить, является ли токен валидным session token
     * 
     * @param token JWT токен
     * @return true если это валидный, не истёкший session token
     * 
     * @note Используется для операций управления сессией (select-account, refresh)
     */
    virtual bool isValidSessionToken(const std::string& token) = 0;

    // ========================================================================
    // ОБНОВЛЕНИЕ ТОКЕНА
    // ========================================================================

    /**
     * @brief Обновить session token
     * 
     * Выдаёт новый session token с полным временем жизни.
     * Старый токен становится невалидным.
     * 
     * @param sessionToken Текущий валидный session token
     * 
     * @return RefreshResult:
     *   - success=true — новый sessionToken выдан
     *   - success=false, error="Invalid session token" — токен невалиден
     * 
     * @note Работает ТОЛЬКО с session token
     * @note Access token обновить нельзя — нужно заново вызвать selectAccount()
     */
    virtual RefreshResult refreshSession(const std::string& sessionToken) = 0;

    // ========================================================================
    // ИЗВЛЕЧЕНИЕ ДАННЫХ ИЗ ТОКЕНОВ
    // ========================================================================

    /**
     * @brief Извлечь accountId из access token
     * 
     * @param accessToken Access token
     * @return accountId или nullopt если токен невалиден или это session token
     * 
     * @note Только для access token. Session token не содержит accountId.
     */
    virtual std::optional<std::string> getAccountIdFromToken(
        const std::string& accessToken
    ) = 0;

    /**
     * @brief Извлечь userId из любого токена
     * 
     * @param token JWT токен (session или access)
     * @return userId или nullopt если токен невалиден
     */
    virtual std::optional<std::string> getUserIdFromToken(
        const std::string& token
    ) = 0;

    /**
     * @brief Извлечь username из любого токена
     * 
     * @param token JWT токен (session или access)
     * @return username или nullopt если токен невалиден
     */
    virtual std::optional<std::string> getUsernameFromToken(
        const std::string& token
    ) = 0;

    // ========================================================================
    // ВЫХОД ИЗ СИСТЕМЫ
    // ========================================================================

    /**
     * @brief Выйти из системы (инвалидировать один токен)
     * 
     * Добавляет токен в blacklist. После вызова токен становится невалидным.
     * 
     * @param token JWT токен для инвалидации (session или access)
     * 
     * @note Можно вызывать как с session, так и с access token
     * @note Если передан session token — access токены этой сессии остаются валидными
     * @note Если токен уже невалиден — операция игнорируется (не ошибка)
     */
    virtual void logout(const std::string& token) = 0;

    /**
     * @brief Выйти из всех сессий пользователя
     * 
     * Инвалидирует ВСЕ токены пользователя (session и access).
     * Используется при смене пароля или подозрении на компрометацию.
     * 
     * @param userId ID пользователя
     * 
     * @note Все активные сессии пользователя будут завершены
     * @note Пользователю придётся заново выполнить login
     */
    virtual void logoutAll(const std::string& userId) = 0;
};

} // namespace trading::ports::input
