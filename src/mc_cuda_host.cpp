#include <cstring>
#include "battleship.h"
#include "mc_cuda.h"

#if defined(USE_CUDA)
// Declarations for the real GPU implementations (should match mc_cuda.cu)
extern "C" int cudaAvailable_impl();
extern "C" void monteCarloProbabilitiesGPU_impl(const char boardView[NUM_ROWS][NUM_COLS], const int remaining[NUM_SHIPS], int iterations, int outCounts[NUM_ROWS * NUM_COLS]);

extern "C" int cudaAvailable() {
    return cudaAvailable_impl();
}
extern "C" void monteCarloProbabilitiesGPU(const char boardView[NUM_ROWS][NUM_COLS],
                                            const int remaining[NUM_SHIPS],
                                            int iterations,
                                            int outCounts[NUM_ROWS * NUM_COLS]) {
    monteCarloProbabilitiesGPU_impl(boardView, remaining, iterations, outCounts);
}
#else
extern "C" int cudaAvailable() {
    return 0;
}
extern "C" void monteCarloProbabilitiesGPU(const char boardView[NUM_ROWS][NUM_COLS],
                                            const int remaining[NUM_SHIPS],
                                            int iterations,
                                            int outCounts[NUM_ROWS * NUM_COLS]) {
    std::memset(outCounts, 0, sizeof(int) * NUM_ROWS * NUM_COLS);
}
#endif
