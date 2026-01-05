-- Инструменты (MOEX Blue Chips)
INSERT INTO instruments (figi, ticker, name, currency, lot_size, min_price_increment) VALUES
('BBG004730N88', 'SBER', 'Сбербанк', 'RUB', 10, 100),
('BBG004730RP0', 'GAZP', 'Газпром', 'RUB', 10, 100),
('BBG004731032', 'LKOH', 'Лукойл', 'RUB', 1, 500),
('BBG006L8G4H1', 'YNDX', 'Яндекс', 'RUB', 1, 1000),
('BBG004RVFCY3', 'MGNT', 'Магнит', 'RUB', 1, 500)
ON CONFLICT (figi) DO NOTHING;

-- Котировки
INSERT INTO quotes (figi, bid, ask, last_price, volume) VALUES
('BBG004730N88', 26500, 26550, 26525, 1000000),
('BBG004730RP0', 15800, 15850, 15820, 500000),
('BBG004731032', 710000, 710500, 710200, 100000),
('BBG006L8G4H1', 350000, 350500, 350200, 50000),
('BBG004RVFCY3', 550000, 550500, 550200, 80000)
ON CONFLICT (figi) DO UPDATE SET
   bid = EXCLUDED.bid,
   ask = EXCLUDED.ask,
   last_price = EXCLUDED.last_price,
   volume = EXCLUDED.volume,
   updated_at = NOW();

