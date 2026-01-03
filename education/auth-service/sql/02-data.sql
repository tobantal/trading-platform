-- auth-service/sql/02-data.sql
-- Тестовые данные

-- Тестовый пользователь (password: test123)
INSERT INTO users (user_id, username, email, password_hash, created_at) VALUES
('user-test-001', 'testuser', 'test@example.com', 'hash:test123', NOW())
ON CONFLICT (user_id) DO NOTHING;

-- Sandbox аккаунт для тестового пользователя
INSERT INTO accounts (account_id, user_id, name, type, tinkoff_token_encrypted, created_at) VALUES
('acc-sandbox-001', 'user-test-001', 'Test Sandbox', 'SANDBOX', 'enc:fake-tinkoff-token', NOW())
ON CONFLICT (account_id) DO NOTHING;
