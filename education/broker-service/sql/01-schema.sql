-- broker-service/sql/01-schema.sql
-- Структура таблиц

CREATE TABLE IF NOT EXISTS instruments (
    figi VARCHAR(12) PRIMARY KEY,
    ticker VARCHAR(12) NOT NULL,
    name VARCHAR(128) NOT NULL,
    currency VARCHAR(3) NOT NULL DEFAULT 'RUB',
    lot_size INTEGER NOT NULL DEFAULT 1,
    min_price_increment BIGINT NOT NULL DEFAULT 100,
    created_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS quotes (
    figi VARCHAR(12) PRIMARY KEY REFERENCES instruments(figi),
    bid BIGINT NOT NULL,
    ask BIGINT NOT NULL,
    last_price BIGINT NOT NULL,
    volume BIGINT DEFAULT 0,
    updated_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS broker_orders (
    order_id VARCHAR(64) PRIMARY KEY,
    account_id VARCHAR(64) NOT NULL,
    figi VARCHAR(12) NOT NULL REFERENCES instruments(figi),
    direction VARCHAR(4) NOT NULL,
    quantity INTEGER NOT NULL,
    filled_quantity INTEGER DEFAULT 0,
    price BIGINT NOT NULL,
    order_type VARCHAR(8) NOT NULL,
    status VARCHAR(20) NOT NULL,
    reject_reason VARCHAR(256),
    received_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS broker_positions (
    account_id VARCHAR(64) NOT NULL,
    figi VARCHAR(12) NOT NULL REFERENCES instruments(figi),
    quantity INTEGER NOT NULL,
    avg_price BIGINT NOT NULL,
    updated_at TIMESTAMP DEFAULT NOW(),
    PRIMARY KEY(account_id, figi)
);

CREATE TABLE IF NOT EXISTS broker_balances (
    account_id VARCHAR(64) PRIMARY KEY,
    currency VARCHAR(3) NOT NULL DEFAULT 'RUB',
    available BIGINT NOT NULL DEFAULT 0,
    reserved BIGINT NOT NULL DEFAULT 0,
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Индексы
CREATE INDEX IF NOT EXISTS idx_broker_orders_account_id ON broker_orders(account_id);
CREATE INDEX IF NOT EXISTS idx_broker_orders_status ON broker_orders(status);
CREATE INDEX IF NOT EXISTS idx_broker_orders_figi ON broker_orders(figi);
CREATE INDEX IF NOT EXISTS idx_broker_positions_account_id ON broker_positions(account_id);

-- Функции резервирования
CREATE OR REPLACE FUNCTION reserve_balance(
    p_account_id VARCHAR(64),
    p_amount BIGINT
) RETURNS BOOLEAN AS $$
DECLARE
    v_available BIGINT;
BEGIN
    SELECT available INTO v_available
    FROM broker_balances
    WHERE account_id = p_account_id
    FOR UPDATE;
    
    IF v_available IS NULL THEN
        RETURN FALSE;
    END IF;
    
    IF v_available < p_amount THEN
        RETURN FALSE;
    END IF;
    
    UPDATE broker_balances
    SET available = available - p_amount,
        reserved = reserved + p_amount,
        updated_at = NOW()
    WHERE account_id = p_account_id;
    
    RETURN TRUE;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION commit_reserved(
    p_account_id VARCHAR(64),
    p_amount BIGINT
) RETURNS VOID AS $$
BEGIN
    UPDATE broker_balances
    SET reserved = reserved - p_amount,
        updated_at = NOW()
    WHERE account_id = p_account_id;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION release_reserved(
    p_account_id VARCHAR(64),
    p_amount BIGINT
) RETURNS VOID AS $$
BEGIN
    UPDATE broker_balances
    SET reserved = reserved - p_amount,
        available = available + p_amount,
        updated_at = NOW()
    WHERE account_id = p_account_id;
END;
$$ LANGUAGE plpgsql;
