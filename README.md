# Battleship ML

A C++17 Battleship AI that minimizes shots-to-win by combining four probabilistic signals. Ships are placed biasedly, every shot is scored by blending a learned global heatmap, a live placement-coverage map, Monte Carlo sampling (CPU or CUDA), and tactical bonuses — all tunable via a multi-threaded CLI tuner with grid-search and online learning modes.

## How the AI Works

Every unshot cell is scored by blending four signals before each shot:

**1. Global heatmap** — hit/miss counts from logged past games are converted to per-cell probabilities. High-traffic cells get a persistent prior that survives across rounds in a tournament.

**2. Live placement map** — enumerates every legal arrangement of the remaining ships on the current board and scores each cell by how many of those arrangements cover it. Updated every turn as ships are sunk.

**3. Monte Carlo sampling** — randomly samples thousands of legal ship placements and accumulates cell frequencies. Blended in when ≥ `mcBlendThresholdCells` unknown cells remain. Runs on the GPU when CUDA is available; falls back to CPU automatically.

**4. Tactical bonuses** (`AIWeights` in `src/MLforAI.h`):
- Adjacent-hit bonus — strongly prefer cells next to a confirmed hit
- Axis bonus — once a ship's orientation is known, extend along that line
- Checkerboard parity — ships ≥2 cells long cannot be entirely on one parity; prune half the board in search mode
- Ship-fit bias — penalize cells where no remaining ship can legally land

After a hit, a `TargetState` queue takes over and directs shots along the detected axis until the ship sinks, then the queue resets.

## CUDA Monte Carlo Kernel

`src/mc_cuda.cu` implements the GPU sampler using cuRAND and shared-memory atomics:

- Each CUDA thread handles multiple placement samples independently
- Per-block shared histogram (`s_hist[100]`) collects cell counts using `atomicAdd` in shared memory — avoids global memory contention during sampling
- Block leaders flush shared histograms to global `d_counts` after synchronization
- Kernel launch: `ceil(iterations / 256)` blocks × 256 threads, 400 bytes of shared memory per block
- `cudaAvailable_impl()` checks device count at runtime; the stub in `mc_cuda_stub.cpp` returns 0 so the same binary works without CUDA installed

## Parameter Tuner

`src/tuner.cpp` provides two modes:

**Grid search** — sweeps parameter ranges in parallel across threads, outputs CSV:
```bash
./tuner games=1000 threads=8 \
    alpha=0.65:0.05:0.85 \
    place=1.0:0.5:2.0 \
    adj=0.2:0.2:0.6 \
    mc=0.0:0.5:0.5 \
    > sweep.csv
```
Each combination runs `games` CvC tournaments split across `threads` workers using `std::thread` + `std::atomic`. Output columns: `alphaEarly, placementHitMultiplier, adjHitBonus, mcBlendRatio, games, threads, p1_avg_shots, p2_avg_shots`.

**Online learning** — updates weights after each game using a reward/penalize rule (minimize avg shots-to-win), prints progress every 50 games:
```bash
./tuner games=1000 online=1
```

## Build

### Native (CPU)
```bash
g++ -O3 -std=c++17 -pthread -o tuner \
    src/tuner.cpp src/MLforAI.cpp src/Tournament.cpp \
    src/battleship.cpp src/mc_cuda_stub.cpp
```

### Native (CUDA)
```bash
# Requires CUDA toolkit (nvcc)
./scripts/build_tuner_cuda.sh
# Auto-detects GPU at runtime; falls back to CPU if no device found
```

### WebAssembly (browser visualization)
```bash
# Requires Emscripten
./build.sh
python3 -m http.server 8000   # open http://localhost:8000
```

## Architecture

```
src/battleship.cpp    — game rules: board init, ship placement, shot resolution
src/MLforAI.cpp       — AI scoring pipeline: scoreCell, chooseAIMove, heatmaps,
                        placement enumeration, target tracking
src/Tournament.cpp    — RoundState (one game) + Tournament (N games); per-player
                        observation arrays so each AI only sees what it has shot at
src/tuner.cpp         — CLI: grid-search sweep + online learning
src/mc_cuda.cu        — CUDA Monte Carlo kernel (cuRAND + shared-memory atomics)
src/mc_cuda_stub.cpp  — CPU stub; same interface, returns immediately
src/wasm_exports.cpp  — extern "C" bridge for the browser build
wargames.js           — canvas rendering, animation loop, UI controls
```
