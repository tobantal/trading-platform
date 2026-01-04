-- broker-service/sql/02-data.sql
-- Seed данные для broker-service

-- Инструменты (MOEX Blue Chips)
INSERT INTO instruments (figi, ticker, name, currency, lot_size, min_price_increment) VALUES
('BBG004730N88', 'SBER', 'Сбербанк', 'RUB', 10, 100),
('BBG004730RP0', 'GAZP', 'Газпром', 'RUB', 10, 100),
('BBG004731032', 'LKOH', 'Лукойл', 'RUB', 1, 500),
('BBG004731354', 'ROSN', 'Роснефть', 'RUB', 1, 100),
('BBG004S68614', 'ALRS', 'Алроса', 'RUB', 10, 10)
ON CONFLICT (figi) DO NOTHING;

-- Котировки (цены в копейках: 26500 = 265.00 RUB)
INSERT INTO quotes (figi, bid, ask, last_price, volume) VALUES
('BBG004730N88', 26500, 26550, 26525, 1000000),
('BBG004730RP0', 15800, 15850, 15820, 500000),
('BBG004731032', 710000, 710500, 710200, 100000),
('BBG004731354', 57500, 57600, 57550, 200000),
('BBG004S68614', 7200, 7250, 7220, 300000)
ON CONFLICT (figi) DO UPDATE SET
    bid = EXCLUDED.bid,
    ask = EXCLUDED.ask,
    last_price = EXCLUDED.last_price,
    volume = EXCLUDED.volume,
    updated_at = NOW();

-- Балансы (суммы в копейках: 10000000 = 100,000 RUB)
-- ВАЖНО: account_id содержит "sandbox" для совместимости с auto-create
INSERT INTO broker_balances (account_id, currency, available, reserved) VALUES
('acc-sandbox-001', 'RUB', 10000000, 0),
('acc-sandbox-002', 'RUB', 50000000, 0)
ON CONFLICT (account_id) DO UPDATE SET
    available = EXCLUDED.available,
    reserved = EXCLUDED.reserved,
    updated_at = NOW();

-- Позиции для тестового аккаунта
INSERT INTO broker_positions (account_id, figi, ticker, quantity, avg_price, updated_at) VALUES
('acc-sandbox-001', 'BBG004730N88', 'SBER', 100, 26000, NOW()),
('acc-sandbox-001', 'BBG004730RP0', 'GAZP', 50, 15500, NOW())
ON CONFLICT (account_id, figi) DO UPDATE SET
    quantity = EXCLUDED.quantity,
    avg_price = EXCLUDED.avg_price,
    updated_at = NOW();

