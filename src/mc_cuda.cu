#include <cuda.h>
#include <curand_kernel.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "mc_cuda.h"

// Prototype GPU implementation using cuRAND.
// This is a best-effort prototype to demonstrate GPU acceleration
// of the Monte-Carlo sampler. It makes simplifying assumptions and
// is not optimized for production. Use as a starting point.

__device__ inline bool shipFitsAtGPU(const char *sample, int r, int c, int len, bool horiz) {
    const int R = 10, C = 10;
    if (horiz) {
        if (c + len > C) return false;
        for (int k = 0; k < len; ++k) if (sample[r * C + (c + k)] != 0 && sample[r * C + (c + k)] == 'm') return false;
    } else {
        if (r + len > R) return false;
        for (int k = 0; k < len; ++k) if (sample[(r + k) * C + c] != 0 && sample[(r + k) * C + c] == 'm') return false;
    }
    return true;
}

// Optimized kernel: each block keeps a shared histogram of placements,
// threads perform multiple samples and update the shared histogram using
// atomicAdd on shared memory; after finishing, block 0 reduces shared
// histogram to global memory with atomic adds (one per cell per block).
__global__ void MonteCarloKernel(const char *boardViewFlat, const int *shipSizes, int shipCount, int iterations, int *d_counts, unsigned long long seedBase) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    int gridSize = gridDim.x * blockDim.x;

    // determine number of samples per thread to cover `iterations`
    int samplesPerThread = (iterations + gridSize - 1) / gridSize;

    extern __shared__ int s_hist[]; // size should be 100 (passed at launch)

    // initialize shared histogram to zero (stride by threads)
    for (int i = threadIdx.x; i < 100; i += blockDim.x) s_hist[i] = 0;
    __syncthreads();

    // init curand
    curandStatePhilox4_32_10_t state;
    curand_init(seedBase + tid, /*seq*/ 0, /*offset*/ 0, &state);

    // per-thread loop: each thread performs multiple sample attempts
    for (int sidx = 0; sidx < samplesPerThread; ++sidx) {
        int sampleIndex = tid + sidx * gridSize;
        if (sampleIndex >= iterations) break;

        // local sample board copy
        char sample[100];
        for (int i = 0; i < 100; ++i) sample[i] = boardViewFlat[i];

        bool ok = true;
        for (int s = 0; s < shipCount; ++s) {
            int len = shipSizes[s];
            bool placed = false;
            for (int attempt = 0; attempt < 200 && !placed; ++attempt) {
                bool horiz = (curand(&state) & 1ULL) != 0ULL;
                int r = curand(&state) % 10;
                int c = curand(&state) % 10;
                if (!shipFitsAtGPU(sample, r, c, len, horiz)) continue;
                bool conflict = false;
                for (int k = 0; k < len; ++k) {
                    int nr = r + (horiz ? 0 : k);
                    int nc = c + (horiz ? k : 0);
                    if (boardViewFlat[nr * 10 + nc] == 'm') { conflict = true; break; }
                }
                if (conflict) continue;
                for (int k = 0; k < len; ++k) {
                    int nr = r + (horiz ? 0 : k);
                    int nc = c + (horiz ? k : 0);
                    sample[nr * 10 + nc] = 'S';
                }
                placed = true;
            }
            if (!placed) { ok = false; break; }
        }
        if (!ok) continue;

        // Update shared histogram (atomic in shared memory)
        for (int i = 0; i < 100; ++i) if (sample[i] == 'S') atomicAdd(&s_hist[i], 1);
    }

    __syncthreads();

    // Block 0 (per-block leader) reduces shared histogram to global counts
    if (threadIdx.x == 0) {
        for (int i = 0; i < 100; ++i) {
            int v = s_hist[i];
            if (v) atomicAdd(&d_counts[i], v);
        }
    }
}

extern "C" void monteCarloProbabilitiesGPU_impl(const char boardView[NUM_ROWS][NUM_COLS],
                                            const int remaining[NUM_SHIPS],
                                            int iterations,
                                            int outCounts[NUM_ROWS * NUM_COLS]) {
    // Flatten boardView
    char h_board[100];
    for (int r = 0; r < NUM_ROWS; ++r) for (int c = 0; c < NUM_COLS; ++c) h_board[r*10 + c] = boardView[r][c];

    // Prepare ship sizes vector
    int h_ships[NUM_SHIPS]; int shipCount = 0;
    for (int i = 0; i < NUM_SHIPS; ++i) if (remaining[i] > 0) h_ships[shipCount++] = remaining[i];
    if (shipCount == 0) {
        for (int i = 0; i < 100; ++i) outCounts[i] = 0;
        return;
    }

    // Allocate device memory
    char *d_board = nullptr;
    int *d_ships = nullptr;
    int *d_counts = nullptr;
    cudaMalloc(&d_board, 100 * sizeof(char));
    cudaMalloc(&d_ships, shipCount * sizeof(int));
    cudaMalloc(&d_counts, 100 * sizeof(int));
    cudaMemset(d_counts, 0, 100 * sizeof(int));
    cudaMemcpy(d_board, h_board, 100, cudaMemcpyHostToDevice);
    cudaMemcpy(d_ships, h_ships, shipCount * sizeof(int), cudaMemcpyHostToDevice);

    // Launch kernel: one thread per sample
    int threadsPerBlock = 256;
    int blocks = (iterations + threadsPerBlock - 1) / threadsPerBlock;
    unsigned long long seedBase = (unsigned long long)clock();
    int sharedBytes = 100 * sizeof(int);
    MonteCarloKernel<<<blocks, threadsPerBlock, sharedBytes>>>(d_board, d_ships, shipCount, iterations, d_counts, seedBase);
    cudaDeviceSynchronize();

    // Copy back counts
    cudaMemcpy(outCounts, d_counts, 100 * sizeof(int), cudaMemcpyDeviceToHost);

    // Cleanup
    cudaFree(d_board);
    cudaFree(d_ships);
    cudaFree(d_counts);
}

extern "C" int cudaAvailable_impl() {
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    if (err != cudaSuccess) return 0;
    return count > 0 ? 1 : 0;
}
