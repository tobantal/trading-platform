# Build stage
FROM gcc:12 AS builder

WORKDIR /app

# Install dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    libpq-dev \
    libssl-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# Copy root CMakeLists.txt and auth-service
COPY CMakeLists.txt .
COPY auth-service/ auth-service/

# Build (only auth-service)
RUN mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
                -DBUILD_TESTS=OFF \
                -DBUILD_TRADING_SERVICE=OFF \
                -DBUILD_BROKER_SERVICE=OFF \
    && make -j$(nproc) auth-service

# Runtime stage
FROM debian:bookworm-slim

WORKDIR /app

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libpq5 \
    libssl3 \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Copy binary
COPY --from=builder /app/build/auth-service/auth-service .

# Copy config if exists
COPY auth-service/config.json* ./

EXPOSE 8081

HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:8081/health || exit 1

CMD ["./auth-service"]
