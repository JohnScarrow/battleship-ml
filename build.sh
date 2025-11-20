#!/usr/bin/env bash
set -euo pipefail
mkdir -p dist
emcc \
  src/battleship.cpp \
  src/MLforAI.cpp \
  src/Tournament.cpp \
  src/wasm_exports.cpp \
  -O2 -std=c++17 \
  -s WASM=1 \
  -s EXPORTED_FUNCTIONS='["_startTournament","_tickTournament","_isTournamentDone","_getBoardSnapshot","_getHeatmapSnapshot","_getPlayer1BoardSnapshot","_getPlayer2BoardSnapshot","_getPlayer1HeatmapSnapshot","_getPlayer2HeatmapSnapshot","_setAIWeightsFromArray","_getAIWeightsToArray","_makePlayerMove","_isPlayerTurn","_advanceAITurn"]' \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","HEAPF32"]' \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -o dist/battleship.js

echo "Build complete: dist/battleship.js + dist/battleship.wasm"
