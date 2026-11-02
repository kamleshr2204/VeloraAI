# VeloraAI – AI Chatbot for Smarter Learning

A study-companion chatbot with a **C++17 backend** and a plain HTML/CSS/JS web page.
Built for an SY B.Tech **OOPS** course: the backend is designed around classes, inheritance,
polymorphism, templates, exceptions and smart pointers.

> New to the code? Start with **[docs/OOP_GUIDE.md](docs/OOP_GUIDE.md)**.

## What it does

* Remembers the whole conversation and streams answers word by word (with a Stop button).
* Searches the live web, reads pages, does exact maths, knows the date (the AI chooses the tool itself).
* Markdown, tables, highlighted code with Copy buttons, LaTeX maths.
* Chats are saved in your browser; export, regenerate, retry.

## Project layout

```
VeloraAI/
├── backend/        C++ source (core, util, search, tools, ai, server)
├── frontend/       index.html, style.css, script.js
├── tests/          unit tests (no internet or API key needed)
├── docs/           OOP_GUIDE.md
├── CMakeLists.txt
└── start.sh / start.bat
```

## Build & run

**Linux / macOS**
```bash
sudo apt install g++ libssl-dev nlohmann-json3-dev   # macOS: brew install openssl nlohmann-json
./backend/build.sh
export OPENAI_API_KEY=sk-...
./start.sh                                           # opens http://localhost:8080
```

**Windows** – install [MSYS2](https://www.msys2.org/), then in the *UCRT64* terminal:
```
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-openssl mingw-w64-ucrt-x86_64-nlohmann-json
```
Add `C:\msys64\ucrt64\bin` to PATH, double-click `backend\build.bat`, then `start.bat`.

**CMake (any OS)** – downloads nlohmann/json automatically if it is not installed:
```bash
cmake -S . -B build && cmake --build build -j
```

## Run the tests

```bash
./tests/run_tests.sh                 # or:  cmake --build build --target velora_tests && ./build/velora_tests
```
The tests use a fake AI (`MockProvider`) and fake search engines, so they need no internet.

## Settings (environment variables)

| Variable | Meaning | Default |
|---|---|---|
| `OPENAI_API_KEY` | your key (required for OpenAI) | – |
| `VELORA_MODEL` | model name, e.g. `gpt-4.1` for harder questions | `gpt-4.1-mini` |
| `TAVILY_API_KEY` | optional (free tier at tavily.com): better web search | DuckDuckGo, then Wikipedia |
| `OPENAI_BASE_URL` | any OpenAI-compatible server, e.g. `http://localhost:11434/v1` for **Ollama** (no key needed) | OpenAI |
| `VELORA_PORT` | port | `8080` |
| `VELORA_HOST` | address to listen on | `127.0.0.1` |
| `VELORA_FRONTEND` | path to the frontend folder | auto-detected |

## Roadmap

1. ✅ **OOP refactor** (this step)
2. ⏳ Learning features: study modes, quizzes, flashcards, progress
3. ⏳ Deploy on Render (Docker, `PORT`, `0.0.0.0`, allowed origin, access code)

## Good to know

* No chatbot is always right. VeloraAI shows its sources so you can check.
* Your conversation is sent to the AI provider you configure. Chats are stored only in your browser.
* The page loads Markdown / code-colour / maths libraries from cdnjs. Offline it shows plain text.
