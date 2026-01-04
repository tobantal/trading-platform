-- auth-service/sql/02-data.sql
-- Seed данные для auth-service

-- Тестовый пользователь (password: test123)
INSERT INTO users (user_id, username, email, password_hash, created_at) VALUES
('user-001', 'testuser', 'test@example.com', 'hash:test123', NOW())
ON CONFLICT (user_id) DO NOTHING;

-- Sandbox аккаунт (ID содержит "sandbox" — критично для broker auto-create!)
INSERT INTO accounts (account_id, user_id, name, type, tinkoff_token_encrypted, created_at) VALUES
('acc-sandbox-001', 'user-001', 'Test Sandbox', 'SANDBOX', 'enc:fake-token', NOW())
ON CONFLICT (account_id) DO NOTHING;

