-- trading-service/sql/03-clean.sql
-- Очистка данных (порядок важен из-за FK)

TRUNCATE TABLE executions CASCADE;
TRUNCATE TABLE orders CASCADE;
TRUNCATE TABLE portfolio_positions CASCADE;
TRUNCATE TABLE balances CASCADE;
TRUNCATE TABLE idempotency_keys CASCADE;
