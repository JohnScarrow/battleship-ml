// Stub implementations for CUDA functions (not available in WASM)
extern "C" int cudaAvailable() {
    return 0; // CUDA not available in browser
}

extern "C" void monteCarloProbabilitiesGPU(
    const char boardView[10][10],
    const int remaining[5],
    int iterations,
    int outCounts[100]
) {
    // No-op: CUDA GPU acceleration not available in WASM
    // CPU fallback will be used instead
    for (int i = 0; i < 100; ++i) outCounts[i] = 0;
}
