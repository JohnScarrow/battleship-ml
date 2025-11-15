# Battleship ML - WebAssembly Demo

A machine learning Battleship game compiled to WebAssembly for browser visualization.

This is a demo of battleship using different ML functions to minimize turns to victory.

## Features

- **AI vs AI tournaments** with configurable rounds
- **Live heatmap visualization** showing AI probability calculations
- **Progressive speed ramp** - starts slow, accelerates to max speed
- **Pure C++ logic** compiled to WASM (no multithreading, runs sequentially)

## Build Instructions

Requires [Emscripten](https://emscripten.org/docs/getting_started/downloads.html):

```bash
# Build the project
./build.sh
```

This generates:
- `dist/battleship.js` - ES6 module wrapper
- `dist/battleship.wasm` - Compiled C++ game logic

## Run Locally

```bash
# Serve with Python
python3 -m http.server 8000

# Or with Node
npx http-server -p 8000
```

Then open: http://localhost:8000

## Deploy to GitHub Pages

1. Commit `dist/battleship.js` and `dist/battleship.wasm`
2. Push to `main` branch
3. Enable GitHub Pages in repository settings
4. Visit: `https://<username>.github.io/battleship-ml/`

## Architecture

- **C++ Core** (`src/`): Game logic, AI, tournament management
- **WASM Exports** (`wasm_exports.cpp`): Five extern "C" functions:
  - `startTournament(mode, rounds)` - Initialize tournament
  - `tickTournament()` - Advance one move, return log string
  - `isTournamentDone()` - Check if complete
  - `getBoardSnapshot()` - Get 10×10 board state (floats)
  - `getHeatmapSnapshot()` - Get 10×10 probability heatmap
- **JavaScript** (`index.html`): Canvas rendering, animation loop, UI

## Canvas Legend

- 🟢 **Green** = Hit
- 🔴 **Red** = Miss
- 🔵 **Blue gradient** = AI probability heatmap (darker = higher probability)

