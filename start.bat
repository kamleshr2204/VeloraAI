@echo off
REM Starts VeloraAI using local Ollama AI.

cd /d "%~dp0backend"

if not exist velora.exe (
    echo velora.exe not found. Run backend\build.bat first.
    pause
    exit /b 1
)

set "OPENAI_BASE_URL=http://localhost:11434/v1"
set "OPENAI_API_KEY=ollama"
set "VELORA_MODEL=llama3.2:3b"

start "" /min cmd /c "ping -n 3 127.0.0.1 >nul & start http://localhost:8080"

velora.exe
pause