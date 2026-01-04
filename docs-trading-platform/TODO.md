### 3.3 Broker Service

```
broker-service/
├── CMakeLists.txt
├── Dockerfile
├── config.json
├── include/
│   ├── BrokerApp.hpp                            ← NEW
│   ├── adapters/
│   │   ├── primary/
│   │   │   ├── HealthHandler.hpp                ← из hw08
│   │   │   ├── MetricsHandler.hpp               ← из MVP
│   │   │   ├── QuotesHandler.hpp                ← NEW
│   │   │   └── InstrumentsHandler.hpp           ← NEW
│   │   └── secondary/
│   │       ├── PostgresInstrumentRepository.hpp ← NEW
│   │       ├── PostgresQuoteRepository.hpp      ← NEW
│   │       ├── PostgresBrokerOrderRepository.hpp← NEW
│   │       ├── PostgresBrokerPositionRepository.hpp ← NEW
│   │       ├── PostgresBrokerBalanceRepository.hpp  ← NEW
│   │       ├── RabbitMQAdapter.hpp              ← из hw08
│   │       ├── DbSettings.hpp                   ← из hw08
│   │       └── RabbitMQSettings.hpp             ← из hw08
│   ├── application/
│   │   ├── OrderExecutor.hpp                    ← NEW (из MVP OrderProcessor)
│   │   ├── PriceSimulator.hpp                   ← из MVP
│   │   ├── OrderEventHandler.hpp                ← NEW
│   │   └── QuoteService.hpp                     ← NEW
│   ├── domain/
│   │   ├── Instrument.hpp                       ← из MVP (+min_price_increment)
│   │   ├── Quote.hpp                            ← из MVP
│   │   ├── BrokerOrder.hpp                      ← NEW
│   │   ├── BrokerPosition.hpp                   ← NEW
│   │   └── BrokerBalance.hpp                    ← NEW
│   └── ports/
│       ├── input/
│       │   └── IQuoteService.hpp                ← NEW
│       └── output/
│           ├── IInstrumentRepository.hpp        ← NEW
│           ├── IQuoteRepository.hpp             ← NEW
│           ├── IBrokerOrderRepository.hpp       ← NEW
│           ├── IBrokerPositionRepository.hpp    ← NEW
│           ├── IBrokerBalanceRepository.hpp     ← NEW
│           ├── IEventPublisher.hpp              ← из hw08
│           └── IEventConsumer.hpp               ← из hw08
├── src/
│   ├── BrokerApp.cpp
│   ├── RabbitMQAdapter.cpp
│   └── main.cpp
├── sql/
│   └── init.sql
└── tests/
    └── OrderExecutorTest.cpp
```

====== Пересборка trading-service =====


# 1. Пересобрать и запушить образ
docker build -t tobantal/trading-service:latest -f trading-service/Dockerfile .

docker push tobantal/trading-service:latest

# 2. Перезапустить deployment (принудительный pull нового образа)
kubectl rollout restart deployment/trading-service -n trading

# 3. Смотреть статус
kubectl get pods -n trading -l app=trading-service -w

# 4. Логи
kubectl logs -f deployment/trading-service -n trading

# 5 Test
newman run postman/trading-service.postman_collection.json 

----
