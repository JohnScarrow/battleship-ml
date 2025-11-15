#!/usr/bin/env bash
set -euo pipefail

# Build the native tuner with CUDA support (single binary `tuner`).
# Requires: nvcc, g++ and CUDA toolkit installed.


echo "Building tuner with CUDA support..."
rm -rf build
mkdir -p build





# Compile CUDA object (mc_cuda) with -dc to ensure host symbols are linked
nvcc -std=c++17 -O3 -Xcompiler -fPIC -dc src/mc_cuda.cu -o build/mc_cuda.o

# Compile other sources with g++ (use -std=c++17 for C++17 features)
g++ -std=c++17 -O3 -fPIC -pthread -DUSE_CUDA -c src/battleship.cpp -o build/battleship.o
g++ -std=c++17 -O3 -fPIC -pthread -DUSE_CUDA -c src/MLforAI.cpp -o build/MLforAI.o
g++ -std=c++17 -O3 -fPIC -pthread -DUSE_CUDA -c src/Tournament.cpp -o build/Tournament.o
g++ -std=c++17 -O3 -fPIC -pthread -DUSE_CUDA -c src/tuner.cpp -o build/tuner.o
g++ -std=c++17 -O3 -fPIC -pthread -DUSE_CUDA -c src/mc_cuda_host.cpp -o build/mc_cuda_host.o

# Link all objects explicitly (including CUDA object) into a single `tuner` binary using nvcc
nvcc -std=c++17 -O3 -Xcompiler -fPIC \
	build/battleship.o \
	build/MLforAI.o \
	build/Tournament.o \
	build/tuner.o \
	build/mc_cuda.o \
	build/mc_cuda_host.o \
	-o tuner -lcudart -lcurand -lpthread

if [ -x ./tuner ]; then
	echo "Built: ./tuner"
	echo "Usage: ./tuner games=1000 threads=8 alpha=0.8 place=2.0 adj=0.2 mc=0.5"
else
	echo "Build failed: ./tuner not created" >&2
	exit 1
fi
