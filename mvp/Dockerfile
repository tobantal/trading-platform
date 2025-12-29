FROM ubuntu:22.04 AS builder

# Устанавливаем зависимости для сборки
RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    git \
    libboost-all-dev \
    libpq-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Удаляем локальные build-артефакты (могут содержать CMakeCache.txt с локальными путями)
RUN rm -rf build/ cmake-build-* mvp/build/ common/build/

# Собираем приложение (без тестов)
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF && \
    cmake --build . --target trading-mvp --config Release -j$(nproc)

# === Runtime стадия ===
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libpq5 \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Бинарник в build/mvp/trading-mvp
COPY --from=builder /app/build/mvp/trading-mvp .

EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:8080/api/v1/health || exit 1

CMD ["./trading-mvp"]
