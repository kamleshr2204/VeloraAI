#!/usr/bin/env bash
# Build VeloraAI on Linux / macOS (no CMake needed).
#   Ubuntu/Debian: sudo apt install g++ libssl-dev nlohmann-json3-dev
#   macOS:         brew install openssl nlohmann-json   (then add -I/-L for brew's openssl)
# This compiles every .cpp file in the backend folders into one program called "velora".
set -e
cd "$(dirname "$0")"
g++ -std=c++17 -O2 -DCPPHTTPLIB_OPENSSL_SUPPORT -I. \
    main.cpp core/*.cpp util/*.cpp search/*.cpp tools/*.cpp ai/*.cpp server/*.cpp \
    -o velora -lssl -lcrypto -lpthread
echo "Built ./velora"
