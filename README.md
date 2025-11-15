# Battleship ML - WebAssembly Demo

A machine learning Battleship game compiled to WebAssembly for browser visualization.

This is a demo of battleship using different ML functions to minimize turns to victory.

## Features

- **AI vs AI tournaments** with configurable rounds
- **Live heatmap visualization** showing AI probability calculations
- **Progressive speed ramp** - starts slow, accelerates to max speed
- **Pure C++ logic** compiled to WASM (no multithreading, runs sequentially)


## Build Instructions

### WebAssembly (Browser Demo)

Requires [Emscripten](https://emscripten.org/docs/getting_started/downloads.html):

```bash
# Build the WASM demo
./build.sh
```

This generates:
- `dist/battleship.js` - ES6 module wrapper
- `dist/battleship.wasm` - Compiled C++ game logic

### Native CLI Tuner (CPU & GPU)

#### CPU-only build
```bash
g++ -O3 -std=c++17 -pthread -o tuner src/tuner.cpp src/MLforAI.cpp src/Tournament.cpp src/battleship.cpp
```

#### CUDA (GPU) build
```bash
# Requires CUDA toolkit (nvcc)
./scripts/build_tuner_cuda.sh
```
This produces a `tuner` binary that will use GPU acceleration if a compatible CUDA device is detected at runtime.

## Usage

### Web Demo (Browser)

```bash
# Serve with Python
python3 -m http.server 8000
# Or with Node
npx http-server -p 8000
```
Then open: http://localhost:8000

### Native CLI Tuner (CPU/GPU)

#### Parameter Sweep Example
```bash
./tuner games=1000 threads=8 alpha=0.7:0.05:0.9 place=1.0:0.5:2.0 adj=0.2:0.2:0.6 mc=0.0:0.5:0.5 > sweep.csv
```
This runs a grid search over the specified parameter ranges and outputs results to `sweep.csv`.

#### Online Learning Example
```bash
./tuner games=1000 online=1
```
This enables online learning mode, where the AI weights are updated after each game to minimize shots-to-win. Final weights are printed at the end.

#### GPU Acceleration
- The CUDA build (`./scripts/build_tuner_cuda.sh`) produces a `tuner` binary that will automatically use GPU acceleration for Monte-Carlo sampling if a compatible CUDA device is detected at runtime.
- No special command-line flag is needed; detection is automatic.
- To confirm GPU is being used, look for a startup message or check performance (GPU runs are much faster for large `mcIterations`).

##### Example: Large MC sweep with GPU
```bash
./tuner games=1000 threads=16 alpha=0.8 place=2.0 adj=0.2 mc=0.5 mcIterations=1600 > mc_gpu.csv
```

##### Example: Online learning with GPU
```bash
./tuner games=1000 online=1 mcIterations=1600
```

If no GPU is available, the binary will fall back to CPU mode automatically.

---

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

