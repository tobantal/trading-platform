-- auth-service/sql/03-clean.sql
-- Очистка данных (порядок важен из-за FK)

TRUNCATE TABLE sessions CASCADE;
TRUNCATE TABLE accounts CASCADE;
TRUNCATE TABLE users CASCADE;
