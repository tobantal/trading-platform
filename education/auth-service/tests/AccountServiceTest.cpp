#include <gtest/gtest.h>

#include "application/AccountService.hpp"
#include "mocks/InMemoryAccountRepository.hpp"

using namespace auth;
using namespace auth::tests::mocks;

class AccountServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        accountRepo_ = std::make_shared<InMemoryAccountRepository>();
        accountService_ = std::make_shared<application::AccountService>(accountRepo_);
    }

    void TearDown() override {
        accountRepo_->clear();
    }

    std::shared_ptr<InMemoryAccountRepository> accountRepo_;
    std::shared_ptr<application::AccountService> accountService_;
};

// ============================================
// CREATE ACCOUNT TESTS
// ============================================

TEST_F(AccountServiceTest, CreateAccount_Sandbox) {
    ports::input::CreateAccountRequest request;
    request.name = "My Sandbox";
    request.type = domain::AccountType::SANDBOX;
    request.tinkoffToken = "fake-token";

    auto account = accountService_->createAccount("user-123", request);

    EXPECT_FALSE(account.accountId.empty());
    EXPECT_EQ(account.userId, "user-123");
    EXPECT_EQ(account.name, "My Sandbox");
    EXPECT_EQ(account.type, domain::AccountType::SANDBOX);
    EXPECT_EQ(accountRepo_->size(), 1);
}

TEST_F(AccountServiceTest, CreateAccount_Real) {
    ports::input::CreateAccountRequest request;
    request.name = "Real Account";
    request.type = domain::AccountType::REAL;
    request.tinkoffToken = "real-tinkoff-token";

    auto account = accountService_->createAccount("user-456", request);

    EXPECT_EQ(account.type, domain::AccountType::REAL);
    EXPECT_FALSE(account.tinkoffTokenEncrypted.empty());
}

TEST_F(AccountServiceTest, CreateAccount_MultipleAccounts) {
    ports::input::CreateAccountRequest req1{.name = "Sandbox 1", .type = domain::AccountType::SANDBOX};
    ports::input::CreateAccountRequest req2{.name = "Sandbox 2", .type = domain::AccountType::SANDBOX};

    accountService_->createAccount("user-123", req1);
    accountService_->createAccount("user-123", req2);

    auto accounts = accountService_->getUserAccounts("user-123");
    EXPECT_EQ(accounts.size(), 2);
}

// ============================================
// GET ACCOUNTS TESTS
// ============================================

TEST_F(AccountServiceTest, GetUserAccounts_Empty) {
    auto accounts = accountService_->getUserAccounts("user-unknown");
    EXPECT_TRUE(accounts.empty());
}

TEST_F(AccountServiceTest, GetUserAccounts_OnlyOwnAccounts) {
    ports::input::CreateAccountRequest request{.name = "Test", .type = domain::AccountType::SANDBOX};
    
    accountService_->createAccount("user-1", request);
    accountService_->createAccount("user-2", request);
    accountService_->createAccount("user-1", request);

    auto user1Accounts = accountService_->getUserAccounts("user-1");
    auto user2Accounts = accountService_->getUserAccounts("user-2");

    EXPECT_EQ(user1Accounts.size(), 2);
    EXPECT_EQ(user2Accounts.size(), 1);
}

TEST_F(AccountServiceTest, GetAccountById_Found) {
    ports::input::CreateAccountRequest request{.name = "Test", .type = domain::AccountType::SANDBOX};
    auto created = accountService_->createAccount("user-123", request);

    auto found = accountService_->getAccountById(created.accountId);

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->accountId, created.accountId);
    EXPECT_EQ(found->name, "Test");
}

TEST_F(AccountServiceTest, GetAccountById_NotFound) {
    auto found = accountService_->getAccountById("non-existent");
    EXPECT_FALSE(found.has_value());
}

// ============================================
// DELETE ACCOUNT TESTS
// ============================================

TEST_F(AccountServiceTest, DeleteAccount_Success) {
    ports::input::CreateAccountRequest request{.name = "Test", .type = domain::AccountType::SANDBOX};
    auto created = accountService_->createAccount("user-123", request);

    bool deleted = accountService_->deleteAccount(created.accountId);

    EXPECT_TRUE(deleted);
    EXPECT_EQ(accountRepo_->size(), 0);
}

TEST_F(AccountServiceTest, DeleteAccount_NotFound) {
    bool deleted = accountService_->deleteAccount("non-existent");
    EXPECT_FALSE(deleted);
}

// ============================================
// OWNERSHIP TESTS
// ============================================

TEST_F(AccountServiceTest, IsAccountOwner_True) {
    ports::input::CreateAccountRequest request{.name = "Test", .type = domain::AccountType::SANDBOX};
    auto created = accountService_->createAccount("user-123", request);

    EXPECT_TRUE(accountService_->isAccountOwner("user-123", created.accountId));
}

TEST_F(AccountServiceTest, IsAccountOwner_False_DifferentUser) {
    ports::input::CreateAccountRequest request{.name = "Test", .type = domain::AccountType::SANDBOX};
    auto created = accountService_->createAccount("user-123", request);

    EXPECT_FALSE(accountService_->isAccountOwner("user-456", created.accountId));
}

TEST_F(AccountServiceTest, IsAccountOwner_False_NonExistent) {
    EXPECT_FALSE(accountService_->isAccountOwner("user-123", "non-existent"));
}
