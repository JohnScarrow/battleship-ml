#pragma once
#include "battleship.h"

// GPU-accelerated Monte-Carlo sampler (prototype).
// If compiled and linked with CUDA and `-DUSE_CUDA`, MLforAI will call
// `monteCarloProbabilitiesGPU` instead of the CPU version.

// boardView: flattened 10x10 char array (row-major)
// remaining: array of NUM_SHIPS ints
// iterations: number of random placement samples to draw
// outCounts: caller-allocated int[100] buffer to receive raw counts (not normalized)
extern "C" void monteCarloProbabilitiesGPU(const char boardView[NUM_ROWS][NUM_COLS],
                                            const int remaining[NUM_SHIPS],
                                            int iterations,
                                            int outCounts[NUM_ROWS * NUM_COLS]);

// Returns 1 if a CUDA device is available, 0 otherwise.
extern "C" int cudaAvailable();
