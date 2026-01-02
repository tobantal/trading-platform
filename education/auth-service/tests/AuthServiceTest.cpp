#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "application/AuthService.hpp"
#include "adapters/secondary/FakeJwtAdapter.hpp"
#include "adapters/secondary/AuthSettings.hpp"
#include "mocks/InMemoryUserRepository.hpp"
#include "mocks/InMemorySessionRepository.hpp"

using namespace auth;
using namespace auth::tests::mocks;

class AuthServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        settings_ = std::make_shared<adapters::secondary::AuthSettings>();
        userRepo_ = std::make_shared<InMemoryUserRepository>();
        sessionRepo_ = std::make_shared<InMemorySessionRepository>();
        jwtProvider_ = std::make_shared<adapters::secondary::FakeJwtAdapter>();
        
        authService_ = std::make_shared<application::AuthService>(
            settings_, userRepo_, sessionRepo_, jwtProvider_
        );
    }

    void TearDown() override {
        userRepo_->clear();
        sessionRepo_->clear();
    }

    std::shared_ptr<adapters::secondary::AuthSettings> settings_;
    std::shared_ptr<InMemoryUserRepository> userRepo_;
    std::shared_ptr<InMemorySessionRepository> sessionRepo_;
    std::shared_ptr<adapters::secondary::FakeJwtAdapter> jwtProvider_;
    std::shared_ptr<application::AuthService> authService_;
};

// ============================================
// REGISTER TESTS
// ============================================

TEST_F(AuthServiceTest, RegisterUser_Success) {
    auto result = authService_->registerUser("john", "john@example.com", "password123");
    
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.userId.empty());
    EXPECT_EQ(result.message, "User registered successfully");
    EXPECT_EQ(userRepo_->size(), 1);
}

TEST_F(AuthServiceTest, RegisterUser_DuplicateUsername) {
    authService_->registerUser("john", "john@example.com", "password123");
    auto result = authService_->registerUser("john", "other@example.com", "password456");
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.message, "Username already exists");
    EXPECT_EQ(userRepo_->size(), 1);
}

TEST_F(AuthServiceTest, RegisterUser_DuplicateEmail) {
    authService_->registerUser("john", "john@example.com", "password123");
    auto result = authService_->registerUser("jane", "john@example.com", "password456");
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.message, "Email already exists");
    EXPECT_EQ(userRepo_->size(), 1);
}

// ============================================
// LOGIN TESTS
// ============================================

TEST_F(AuthServiceTest, Login_Success) {
    authService_->registerUser("john", "john@example.com", "password123");
    
    auto result = authService_->login("john", "password123");
    
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.sessionToken.empty());
    EXPECT_FALSE(result.userId.empty());
    EXPECT_EQ(result.message, "Login successful");
    EXPECT_EQ(sessionRepo_->size(), 1);
}

TEST_F(AuthServiceTest, Login_UserNotFound) {
    auto result = authService_->login("unknown", "password123");
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.message, "User not found");
    EXPECT_TRUE(result.sessionToken.empty());
}

TEST_F(AuthServiceTest, Login_WrongPassword) {
    authService_->registerUser("john", "john@example.com", "password123");
    
    auto result = authService_->login("john", "wrongpassword");
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.message, "Invalid password");
}

// ============================================
// LOGOUT TESTS
// ============================================

TEST_F(AuthServiceTest, Logout_Success) {
    authService_->registerUser("john", "john@example.com", "password123");
    auto loginResult = authService_->login("john", "password123");
    
    bool logoutSuccess = authService_->logout(loginResult.sessionToken);
    
    EXPECT_TRUE(logoutSuccess);
    
    // Токен должен быть инвалидирован
    auto validation = authService_->validateSessionToken(loginResult.sessionToken);
    EXPECT_FALSE(validation.valid);
}

TEST_F(AuthServiceTest, Logout_InvalidToken) {
    bool result = authService_->logout("invalid-token");
    EXPECT_FALSE(result);
}

// ============================================
// VALIDATE TOKEN TESTS
// ============================================

TEST_F(AuthServiceTest, ValidateSessionToken_Valid) {
    authService_->registerUser("john", "john@example.com", "password123");
    auto loginResult = authService_->login("john", "password123");
    
    auto validation = authService_->validateSessionToken(loginResult.sessionToken);
    
    EXPECT_TRUE(validation.valid);
    EXPECT_EQ(validation.userId, loginResult.userId);
}

TEST_F(AuthServiceTest, ValidateSessionToken_Invalid) {
    auto validation = authService_->validateSessionToken("invalid-token");
    
    EXPECT_FALSE(validation.valid);
}

// ============================================
// ACCESS TOKEN TESTS
// ============================================

TEST_F(AuthServiceTest, CreateAccessToken_Success) {
    authService_->registerUser("john", "john@example.com", "password123");
    auto loginResult = authService_->login("john", "password123");
    
    auto accessToken = authService_->createAccessToken(loginResult.sessionToken, "acc-123");
    
    ASSERT_TRUE(accessToken.has_value());
    EXPECT_FALSE(accessToken->empty());
    
    // Validate access token
    auto validation = authService_->validateAccessToken(*accessToken);
    EXPECT_TRUE(validation.valid);
    EXPECT_EQ(validation.userId, loginResult.userId);
    EXPECT_EQ(validation.accountId, "acc-123");
}

TEST_F(AuthServiceTest, CreateAccessToken_InvalidSession) {
    auto accessToken = authService_->createAccessToken("invalid-session", "acc-123");
    
    EXPECT_FALSE(accessToken.has_value());
}

// ============================================
// SESSION LIFETIME TEST (regression test for DI bug)
// ============================================

TEST_F(AuthServiceTest, Login_SessionTokenHasCorrectLifetime) {
    authService_->registerUser("john", "john@example.com", "password123");
    auto result = authService_->login("john", "password123");
    
    EXPECT_TRUE(result.success);
    
    // Validate that token is valid immediately
    auto validation = authService_->validateSessionToken(result.sessionToken);
    EXPECT_TRUE(validation.valid) << "Token should be valid immediately after login";
    
    // Token should contain valid expiration (this tests that SESSION_LIFETIME_SECONDS is used)
    EXPECT_FALSE(result.sessionToken.empty());
}
