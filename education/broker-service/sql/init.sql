-- Broker Service Database Schema
-- Схема базы данных для Broker Service

-- ============================================================================
-- ТАБЛИЦЫ
-- ============================================================================

-- Инструменты (акции, облигации)
CREATE TABLE IF NOT EXISTS instruments (
    figi VARCHAR(12) PRIMARY KEY,
    ticker VARCHAR(12) NOT NULL,
    name VARCHAR(128) NOT NULL,
    currency VARCHAR(3) NOT NULL DEFAULT 'RUB',
    lot_size INTEGER NOT NULL DEFAULT 1,
    min_price_increment BIGINT NOT NULL DEFAULT 100,  -- в копейках
    created_at TIMESTAMP DEFAULT NOW()
);

-- Котировки
CREATE TABLE IF NOT EXISTS quotes (
    figi VARCHAR(12) PRIMARY KEY REFERENCES instruments(figi),
    bid BIGINT NOT NULL,        -- цена покупки в копейках
    ask BIGINT NOT NULL,        -- цена продажи в копейках  
    last_price BIGINT NOT NULL, -- последняя цена в копейках
    volume BIGINT DEFAULT 0,    -- объём торгов
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Ордера брокера
CREATE TABLE IF NOT EXISTS broker_orders (
    order_id VARCHAR(64) PRIMARY KEY,
    account_id VARCHAR(64) NOT NULL,
    figi VARCHAR(12) NOT NULL REFERENCES instruments(figi),
    direction VARCHAR(4) NOT NULL,    -- BUY, SELL
    quantity INTEGER NOT NULL,        -- количество лотов
    filled_quantity INTEGER DEFAULT 0,-- исполнено лотов
    price BIGINT NOT NULL,            -- цена в копейках
    order_type VARCHAR(8) NOT NULL,   -- MARKET, LIMIT
    status VARCHAR(20) NOT NULL,      -- PENDING, FILLED, PARTIALLY_FILLED, REJECTED, CANCELLED
    reject_reason VARCHAR(256),
    received_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Позиции (по инструментам)
CREATE TABLE IF NOT EXISTS broker_positions (
    account_id VARCHAR(64) NOT NULL,
    figi VARCHAR(12) NOT NULL REFERENCES instruments(figi),
    quantity INTEGER NOT NULL,        -- количество штук (не лотов!)
    avg_price BIGINT NOT NULL,        -- средняя цена покупки в копейках
    updated_at TIMESTAMP DEFAULT NOW(),
    PRIMARY KEY(account_id, figi)
);

-- Балансы с резервированием
CREATE TABLE IF NOT EXISTS broker_balances (
    account_id VARCHAR(64) PRIMARY KEY,
    currency VARCHAR(3) NOT NULL DEFAULT 'RUB',
    available BIGINT NOT NULL DEFAULT 0,  -- доступно для торговли (копейки)
    reserved BIGINT NOT NULL DEFAULT 0,   -- зарезервировано под ордера (копейки)
    updated_at TIMESTAMP DEFAULT NOW()
);

-- ============================================================================
-- ИНДЕКСЫ
-- ============================================================================

CREATE INDEX IF NOT EXISTS idx_broker_orders_account_id ON broker_orders(account_id);
CREATE INDEX IF NOT EXISTS idx_broker_orders_status ON broker_orders(status);
CREATE INDEX IF NOT EXISTS idx_broker_orders_figi ON broker_orders(figi);
CREATE INDEX IF NOT EXISTS idx_broker_positions_account_id ON broker_positions(account_id);

-- ============================================================================
-- ТЕСТОВЫЕ ДАННЫЕ
-- ============================================================================

-- Инструменты (цены в копейках)
INSERT INTO instruments (figi, ticker, name, currency, lot_size, min_price_increment) VALUES
('BBG004730N88', 'SBER', 'Сбербанк', 'RUB', 10, 100),
('BBG004730RP0', 'GAZP', 'Газпром', 'RUB', 10, 100),
('BBG004731032', 'LKOH', 'Лукойл', 'RUB', 1, 500),
('BBG004731354', 'ROSN', 'Роснефть', 'RUB', 1, 100),
('BBG004S68614', 'ALRS', 'Алроса', 'RUB', 10, 10)
ON CONFLICT (figi) DO NOTHING;

-- Котировки (цены в копейках: 26500 = 265.00 руб)
INSERT INTO quotes (figi, bid, ask, last_price, volume) VALUES
('BBG004730N88', 26500, 26550, 26525, 1000000),   -- SBER ~265 руб
('BBG004730RP0', 15800, 15850, 15820, 500000),    -- GAZP ~158 руб
('BBG004731032', 710000, 710500, 710200, 100000), -- LKOH ~7100 руб
('BBG004731354', 57500, 57600, 57550, 200000),    -- ROSN ~575 руб
('BBG004S68614', 7200, 7250, 7220, 300000)        -- ALRS ~72 руб
ON CONFLICT (figi) DO UPDATE SET
    bid = EXCLUDED.bid,
    ask = EXCLUDED.ask,
    last_price = EXCLUDED.last_price,
    volume = EXCLUDED.volume,
    updated_at = NOW();

-- Тестовые балансы (суммы в копейках: 100000000 = 1_000_000 руб)
INSERT INTO broker_balances (account_id, currency, available, reserved) VALUES
('acc-001-sandbox', 'RUB', 100000000, 0),   -- 1_000_000 руб
('acc-001-prod', 'RUB', 50000000, 0),       -- 500_000 руб
('acc-002-sandbox', 'RUB', 10000000, 0),    -- 100_000 руб
('acc-004-sandbox', 'RUB', 1000000000, 0)   -- 10_000_000 руб
ON CONFLICT (account_id) DO UPDATE SET
    available = EXCLUDED.available,
    reserved = EXCLUDED.reserved,
    updated_at = NOW();

-- ============================================================================
-- ФУНКЦИИ ДЛЯ РЕЗЕРВИРОВАНИЯ
-- ============================================================================

-- Функция резервирования средств
CREATE OR REPLACE FUNCTION reserve_balance(
    p_account_id VARCHAR(64),
    p_amount BIGINT
) RETURNS BOOLEAN AS $$
DECLARE
    v_available BIGINT;
BEGIN
    -- Блокируем строку для атомарности
    SELECT available INTO v_available
    FROM broker_balances
    WHERE account_id = p_account_id
    FOR UPDATE;
    
    IF v_available IS NULL THEN
        RETURN FALSE;  -- Аккаунт не найден
    END IF;
    
    IF v_available < p_amount THEN
        RETURN FALSE;  -- Недостаточно средств
    END IF;
    
    UPDATE broker_balances
    SET available = available - p_amount,
        reserved = reserved + p_amount,
        updated_at = NOW()
    WHERE account_id = p_account_id;
    
    RETURN TRUE;
END;
$$ LANGUAGE plpgsql;

-- Функция подтверждения резерва (исполнение ордера)
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

-- Функция освобождения резерва (отмена/отклонение)
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
