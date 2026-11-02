@echo off
REM Build VeloraAI with MSYS2 / MinGW-w64 (UCRT64 or MINGW64 shell must be on PATH).
REM One-time setup, in the MSYS2 UCRT64 terminal:
REM   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-openssl mingw-w64-ucrt-x86_64-nlohmann-json
cd /d "%~dp0"
g++ -std=c++17 -O2 -DCPPHTTPLIB_OPENSSL_SUPPORT -I. main.cpp core\*.cpp util\*.cpp search\*.cpp tools\*.cpp ai\*.cpp server\*.cpp -o velora.exe -lssl -lcrypto -lws2_32 -lcrypt32 -static-libgcc -static-libstdc++
if errorlevel 1 (echo. & echo Build FAILED. See README.md, section "Build". & pause & exit /b 1)
echo. & echo Built velora.exe successfully.
pause
