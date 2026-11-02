#!/usr/bin/env bash
# Starts VeloraAI and opens it in your browser.
cd "$(dirname "$0")/backend"
[ -x ./velora ] || { echo "Run backend/build.sh first."; exit 1; }
if [ -z "$OPENAI_API_KEY" ]; then
  read -rsp "Paste your OpenAI API key: " OPENAI_API_KEY
  export OPENAI_API_KEY
  echo
fi
(sleep 2; xdg-open http://localhost:8080 2>/dev/null || open http://localhost:8080 2>/dev/null) &
./velora
