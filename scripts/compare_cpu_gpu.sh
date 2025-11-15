#!/usr/bin/env bash
set -euo pipefail

CPU_BIN="./tuner_cpu"
GPU_BIN="./tuner"
BUILD_CUDA_SCRIPT="scripts/build_tuner_cuda.sh"


# Parameters (override via env vars if desired)
GAMES=${GAMES:-1000}
THREADS=${THREADS:-16}
ALPHA=${ALPHA:-0.800}
PLACE=${PLACE:-2.000}
ADJ=${ADJ:-0.200}
MC=${MC:-0.500}

echo "Compare CPU vs GPU tuner (games=${GAMES})"

echo "Building CPU-only tuner (./tuner_cpu)..."
g++ -std=c++17 -O3 -pthread src/battleship.cpp src/MLforAI.cpp src/Tournament.cpp src/tuner.cpp src/mc_cuda_stub.cpp -o "$CPU_BIN"

if command -v nvcc >/dev/null 2>&1; then
  echo "nvcc found — building GPU tuner"
  chmod +x "$BUILD_CUDA_SCRIPT"
  "$BUILD_CUDA_SCRIPT"
else
  echo "nvcc not found — GPU build skipped"
fi

if [ ! -x "$CPU_BIN" ]; then
  echo "CPU binary $CPU_BIN missing or not executable" >&2
  exit 1
fi
if [ ! -x "$GPU_BIN" ]; then
  echo "GPU binary $GPU_BIN missing or not executable" >&2
  echo "You can build it with: $BUILD_CUDA_SCRIPT" >&2
  exit 1
fi

echo "Running CPU benchmark..."
CPU_OUT=cpu_out.txt
start=$(date +%s.%N)
$CPU_BIN games=$GAMES threads=$THREADS alpha=$ALPHA place=$PLACE adj=$ADJ mc=$MC > "$CPU_OUT" 2>&1
end=$(date +%s.%N)
cpu_time=$(echo "$end - $start" | bc)

echo "Running GPU benchmark..."
GPU_OUT=gpu_out.txt
start=$(date +%s.%N)
$GPU_BIN games=$GAMES threads=$THREADS alpha=$ALPHA place=$PLACE adj=$ADJ mc=$MC > "$GPU_OUT" 2>&1
end=$(date +%s.%N)
gpu_time=$(echo "$end - $start" | bc)

cpu_line=$(tail -n 1 "$CPU_OUT")
gpu_line=$(tail -n 1 "$GPU_OUT")

cpu_p1=$(echo "$cpu_line" | awk -F, '{print $7}') || cpu_p1="NA"
gpu_p1=$(echo "$gpu_line" | awk -F, '{print $7}') || gpu_p1="NA"

cpu_sha=$(sha256sum "$CPU_OUT" | awk '{print $1}')
gpu_sha=$(sha256sum "$GPU_OUT" | awk '{print $1}')

speedup=$(awk -v c="$cpu_time" -v g="$gpu_time" 'BEGIN{ if (g==0) print "inf"; else printf("%.2f", c/g) }')

cat <<EOF
\n=== Compare Report ===
CPU time: ${cpu_time}s
GPU time: ${gpu_time}s
Speedup (CPU/GPU): ${speedup}x

CPU result: ${cpu_line}
GPU result: ${gpu_line}

CPU p1_avg_shots: ${cpu_p1}
GPU p1_avg_shots: ${gpu_p1}

CPU sha256: ${cpu_sha}
GPU sha256: ${gpu_sha}

Full CPU output: ${CPU_OUT}
Full GPU output: ${GPU_OUT}
EOF

exit 0
