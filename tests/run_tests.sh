#!/usr/bin/env bash
# Builds and runs the unit tests (Linux / macOS).
#   Ubuntu/Debian: sudo apt install g++ libssl-dev nlohmann-json3-dev
set -e
cd "$(dirname "$0")/.."
g++ -std=c++17 -O1 -DCPPHTTPLIB_OPENSSL_SUPPORT -Ibackend \
    tests/test_main.cpp backend/core/*.cpp backend/util/*.cpp backend/search/*.cpp \
    backend/tools/*.cpp backend/ai/*.cpp backend/server/*.cpp \
    -o tests/velora_tests -lssl -lcrypto -lpthread
./tests/velora_tests
