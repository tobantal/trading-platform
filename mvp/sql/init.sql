-- =============================================================================
-- init.sql — Инициализация PostgreSQL для Trading Platform
-- =============================================================================
-- Данные ПОЛНОСТЬЮ совместимы с InMemoryUserRepository и InMemoryAccountRepository
-- 
-- Тестовые пользователи:
-- ┌────────────┬──────────┬────────────┬─────────────────────────────┐
-- │ ID         │ Username │ Password   │ Описание                    │
-- ├────────────┼──────────┼────────────┼─────────────────────────────┤
-- │ user-001   │ trader1  │ password1  │ 2 аккаунта (sandbox + prod) │
-- │ user-002   │ trader2  │ password2  │ 1 аккаунт (sandbox)         │
-- │ user-003   │ newbie   │ password3  │ 0 аккаунтов (новичок)       │
-- │ user-004   │ admin    │ admin123   │ 1 аккаунт (админ)           │
-- └────────────┴──────────┴────────────┴─────────────────────────────┘
-- =============================================================================

-- Удаляем таблицы если существуют (для перезапуска)
DROP TABLE IF EXISTS strategies CASCADE;
DROP TABLE IF EXISTS orders CASCADE;
DROP TABLE IF EXISTS accounts CASCADE;
DROP TABLE IF EXISTS users CASCADE;

-- =============================================================================
-- ТАБЛИЦА: users
-- =============================================================================
CREATE TABLE users (
    id VARCHAR(64) PRIMARY KEY,
    username VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX idx_users_username ON users(username);

-- =============================================================================
-- ТАБЛИЦА: accounts
-- =============================================================================
CREATE TABLE accounts (
    id VARCHAR(64) PRIMARY KEY,
    user_id VARCHAR(64) NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name VARCHAR(255) NOT NULL,
    type VARCHAR(32) NOT NULL CHECK (type IN ('SANDBOX', 'PRODUCTION')),
    broker_account_id VARCHAR(255) DEFAULT '',
    broker_token VARCHAR(1024) DEFAULT '',
    is_active BOOLEAN DEFAULT true,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX idx_accounts_user_id ON accounts(user_id);

-- =============================================================================
-- ТАБЛИЦА: orders
-- =============================================================================
CREATE TABLE orders (
    id VARCHAR(64) PRIMARY KEY,
    account_id VARCHAR(64) NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    figi VARCHAR(32) NOT NULL,
    direction VARCHAR(16) NOT NULL CHECK (direction IN ('BUY', 'SELL')),
    type VARCHAR(16) NOT NULL CHECK (type IN ('MARKET', 'LIMIT')),
    quantity INTEGER NOT NULL CHECK (quantity > 0),
    price_units BIGINT DEFAULT 0,
    price_nano INTEGER DEFAULT 0,
    price_currency VARCHAR(8) DEFAULT 'RUB',
    status VARCHAR(32) NOT NULL CHECK (status IN ('PENDING', 'FILLED', 'PARTIALLY_FILLED', 'CANCELLED', 'REJECTED')),
    broker_order_id VARCHAR(255) DEFAULT '',
    executed_quantity INTEGER DEFAULT 0,
    executed_price_units BIGINT DEFAULT 0,
    executed_price_nano INTEGER DEFAULT 0,
    message TEXT DEFAULT '',
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX idx_orders_account_id ON orders(account_id);
CREATE INDEX idx_orders_status ON orders(status);
CREATE INDEX idx_orders_created_at ON orders(created_at);

-- =============================================================================
-- ТАБЛИЦА: strategies
-- =============================================================================
CREATE TABLE strategies (
    id VARCHAR(64) PRIMARY KEY,
    account_id VARCHAR(64) NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    name VARCHAR(255) NOT NULL,
    type VARCHAR(32) NOT NULL CHECK (type IN ('SMA_CROSSOVER', 'RSI', 'MACD', 'CUSTOM')),
    figi VARCHAR(32) NOT NULL,
    config TEXT DEFAULT '{}',
    status VARCHAR(32) NOT NULL CHECK (status IN ('CREATED', 'RUNNING', 'STOPPED', 'ERROR')),
    error_message TEXT DEFAULT '',
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX idx_strategies_account_id ON strategies(account_id);
CREATE INDEX idx_strategies_status ON strategies(status);

-- =============================================================================
-- ТЕСТОВЫЕ ДАННЫЕ: users
-- =============================================================================
-- ВАЖНО: password_hash = просто пароль (FakeJwtAdapter не хеширует)
-- В продакшене нужен bcrypt!

INSERT INTO users (id, username, password_hash, created_at, updated_at) VALUES
    ('user-001', 'trader1', 'password1', NOW(), NOW()),
    ('user-002', 'trader2', 'password2', NOW(), NOW()),
    ('user-003', 'newbie',  'password3', NOW(), NOW()),
    ('user-004', 'admin',   'admin123',  NOW(), NOW());

-- =============================================================================
-- ТЕСТОВЫЕ ДАННЫЕ: accounts
-- =============================================================================
-- Соответствует InMemoryAccountRepository:
-- - trader1 (user-001): 2 аккаунта (sandbox + prod)
-- - trader2 (user-002): 1 аккаунт (sandbox)
-- - newbie  (user-003): 0 аккаунтов
-- - admin   (user-004): 1 аккаунт (sandbox)

INSERT INTO accounts (id, user_id, name, type, broker_account_id, broker_token, is_active) VALUES
    -- trader1: опытный трейдер с 2 аккаунтами
    ('acc-001-sandbox', 'user-001', 'Sandbox Account',    'SANDBOX',    '', '', true),
    ('acc-001-prod',    'user-001', 'Production Account', 'PRODUCTION', '', '', true),
    
    -- trader2: начинающий с 1 аккаунтом
    ('acc-002-sandbox', 'user-002', 'My Sandbox',         'SANDBOX',    '', '', true),
    
    -- newbie: БЕЗ аккаунтов (для тестирования добавления)
    -- (ничего не вставляем)
    
    -- admin: администратор с sandbox аккаунтом
    ('acc-004-sandbox', 'user-004', 'Admin Sandbox',      'SANDBOX',    '', '', true);

-- =============================================================================
-- ПРОВЕРКА ДАННЫХ
-- =============================================================================

-- Вывод статистики после инициализации
DO $$
DECLARE
    user_count INTEGER;
    account_count INTEGER;
BEGIN
    SELECT COUNT(*) INTO user_count FROM users;
    SELECT COUNT(*) INTO account_count FROM accounts;
    
    RAISE NOTICE '==============================================';
    RAISE NOTICE 'Database initialized successfully!';
    RAISE NOTICE '  Users:    %', user_count;
    RAISE NOTICE '  Accounts: %', account_count;
    RAISE NOTICE '==============================================';
    RAISE NOTICE 'Test credentials:';
    RAISE NOTICE '  trader1 / password1 (2 accounts)';
    RAISE NOTICE '  trader2 / password2 (1 account)';
    RAISE NOTICE '  newbie  / password3 (0 accounts)';
    RAISE NOTICE '  admin   / admin123  (1 account)';
    RAISE NOTICE '==============================================';
END $$;

-- =============================================================================
-- ПОЛЕЗНЫЕ ЗАПРОСЫ ДЛЯ ОТЛАДКИ
-- =============================================================================

-- Все пользователи с количеством аккаунтов:
-- SELECT u.id, u.username, COUNT(a.id) as accounts_count
-- FROM users u
-- LEFT JOIN accounts a ON u.id = a.user_id
-- GROUP BY u.id, u.username;

-- Все аккаунты с именами пользователей:
-- SELECT a.id, a.name, a.type, u.username
-- FROM accounts a
-- JOIN users u ON a.user_id = u.id;

-- Активные ордера по аккаунтам:
-- SELECT a.name, o.figi, o.direction, o.quantity, o.status, o.created_at
-- FROM orders o
-- JOIN accounts a ON o.account_id = a.id
-- WHERE o.status IN ('PENDING', 'PARTIALLY_FILLED')
-- ORDER BY o.created_at DESC;
