-- trading-service/sql/01-schema.sql
-- Структура таблиц

CREATE TABLE IF NOT EXISTS orders (
    order_id VARCHAR(64) PRIMARY KEY,
    account_id VARCHAR(64) NOT NULL,
    user_id VARCHAR(64) NOT NULL,
    figi VARCHAR(12) NOT NULL,
    direction VARCHAR(4) NOT NULL,
    quantity INTEGER NOT NULL,
    filled_quantity INTEGER DEFAULT 0,
    price BIGINT NOT NULL,
    order_type VARCHAR(8) NOT NULL,
    status VARCHAR(20) NOT NULL,
    reject_reason VARCHAR(256),
    executed_price BIGINT,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS executions (
    execution_id VARCHAR(64) PRIMARY KEY,
    order_id VARCHAR(64) REFERENCES orders(order_id),
    quantity INTEGER NOT NULL,
    price BIGINT NOT NULL,
    executed_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS portfolio_positions (
    position_id VARCHAR(64) PRIMARY KEY,
    account_id VARCHAR(64) NOT NULL,
    user_id VARCHAR(64) NOT NULL,
    figi VARCHAR(12) NOT NULL,
    quantity INTEGER NOT NULL,
    avg_price BIGINT NOT NULL,
    updated_at TIMESTAMP DEFAULT NOW(),
    UNIQUE(account_id, figi)
);

CREATE TABLE IF NOT EXISTS balances (
    account_id VARCHAR(64) PRIMARY KEY,
    currency VARCHAR(3) NOT NULL DEFAULT 'RUB',
    available BIGINT NOT NULL DEFAULT 0,
    reserved BIGINT NOT NULL DEFAULT 0,
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Идемпотентность запросов
CREATE TABLE idempotency_keys (
    key VARCHAR(64) PRIMARY KEY,
    response_status INTEGER NOT NULL,
    response_body TEXT,
    created_at TIMESTAMP DEFAULT NOW()
);

-- Индексы
CREATE INDEX IF NOT EXISTS idx_orders_account_id ON orders(account_id);
CREATE INDEX IF NOT EXISTS idx_orders_user_id ON orders(user_id);
CREATE INDEX IF NOT EXISTS idx_orders_status ON orders(status);
CREATE INDEX IF NOT EXISTS idx_executions_order_id ON executions(order_id);
CREATE INDEX IF NOT EXISTS idx_portfolio_account_id ON portfolio_positions(account_id);
