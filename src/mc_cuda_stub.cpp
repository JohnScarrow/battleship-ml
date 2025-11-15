#include <cstring>
#include "battleship.h"

extern "C" int cudaAvailable() {
    return 0; // no CUDA available in this build
}

extern "C" void monteCarloProbabilitiesGPU(const char boardView[NUM_ROWS][NUM_COLS],
                                             const int remaining[NUM_SHIPS],
                                             int iterations,
                                             int outCounts[NUM_ROWS * NUM_COLS]) {
    // Provide a deterministic fallback: zero the output counts so CPU path is used.
    std::memset(outCounts, 0, sizeof(int) * NUM_ROWS * NUM_COLS);
}
