
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    libssl-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target velora -j2

EXPOSE 10000

CMD ["sh", "-c", "VELORA_HOST=0.0.0.0 VELORA_PORT=${PORT:-10000} ./backend/velora"]
 
