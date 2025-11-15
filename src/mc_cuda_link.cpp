extern "C" int cudaAvailable();
extern "C" void monteCarloProbabilitiesGPU(const char boardView[10][10], const int remaining[5], int iterations, int outCounts[100]);
void __link_cuda_symbols() {
    (void)cudaAvailable;
    (void)monteCarloProbabilitiesGPU;
}
