-- broker-service/sql/03-clean.sql
-- Очистка данных (порядок важен из-за FK)

TRUNCATE TABLE broker_orders CASCADE;
TRUNCATE TABLE broker_positions CASCADE;
TRUNCATE TABLE broker_balances CASCADE;
TRUNCATE TABLE quotes CASCADE;
TRUNCATE TABLE instruments CASCADE;
