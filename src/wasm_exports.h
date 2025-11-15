#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// Start a new tournament
void startTournament(int mode, int totalRounds);
// Advance one move; returns short log string
const char* tickTournament();
// Returns 1 when tournament finished
int isTournamentDone();
// 100 floats board snapshot (0 empty, 1 hit, -1 miss)
const float* getBoardSnapshot();
// 100 floats heatmap probabilities (0.0-1.0)
const float* getHeatmapSnapshot();

#ifdef __cplusplus
}
#endif
