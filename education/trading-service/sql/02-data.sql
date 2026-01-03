-- trading-service/sql/02-data.sql
-- Тестовые данные

-- Начальный баланс для тестового sandbox аккаунта (100,000 руб в копейках)
INSERT INTO balances (account_id, currency, available, reserved, updated_at) VALUES
('acc-sandbox-001', 'RUB', 10000000, 0, NOW())
ON CONFLICT (account_id) DO UPDATE SET
    available = EXCLUDED.available,
    reserved = EXCLUDED.reserved,
    updated_at = NOW();
