FROM ubuntu:22.04 AS builder

# Устанавливаем зависимости для сборки
RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    git \
    libboost-all-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Собираем приложение
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --config Release

# === Runtime стадия ===
FROM ubuntu:22.04

# Только runtime зависимости
RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Копируем собранный бинарник
COPY --from=builder /app/build/trading-helloworld .

# Порт
EXPOSE 8080

# Запуск
CMD ["./trading-helloworld"]