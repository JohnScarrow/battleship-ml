#include "wasm_exports.h"
#include "Tournament.h"
#include "MLforAI.h"
#include <cstdlib>
#include <ctime>

static Tournament gTournament;

extern "C" {

void startTournament(int mode, int totalRounds) {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    gTournament.start(mode, totalRounds);
}

const char* tickTournament() {
    return gTournament.tick();
}

int isTournamentDone() {
    return gTournament.done();
}

const float* getBoardSnapshot() {
    return gTournament.snapshotBoard();
}

const float* getHeatmapSnapshot() {
    return gTournament.getHeatmapSnapshot();
}

const float* getPlayer1BoardSnapshot() {
    return gTournament.snapshotPlayer1Board();
}

const float* getPlayer2BoardSnapshot() {
    return gTournament.snapshotPlayer2Board();
}

const float* getPlayer1HeatmapSnapshot() {
    return gTournament.getPlayer1Heatmap();
}

const float* getPlayer2HeatmapSnapshot() {
    return gTournament.getPlayer2Heatmap();
}
}
// Set AI weights from a float array. Caller must pass an array of at least 16 floats.
extern "C" void setAIWeightsFromArray(const float* arr) {
    if (!arr) return;
    AIWeights w;
    w.globalAlphaEarly = arr[0];
    w.globalAlphaLate  = arr[1];
    w.liveDecayFactor  = arr[2];
    w.tacticalLiveBonus = arr[3];
    w.parityBonus       = arr[4];
    w.parityPenalty     = arr[5];
    w.adjHitBonus       = arr[6];
    w.adjLineBonus      = arr[7];
    w.diagHitBonus      = arr[8];
    w.fitScoreNearAdjFactor = arr[9];
    w.fitScoreBaseFactor    = arr[10];
    w.noFitPenalty      = arr[11];
    w.placementHitMultiplier = arr[12];
    w.mcIterations      = static_cast<int>(arr[13]);
    w.mcBlendRatio      = arr[14];
    w.mcBlendThresholdCells = static_cast<int>(arr[15]);
    setAIWeights(w);
}

// Fill caller-provided float array (length >=16) with the current AI weights.
extern "C" void getAIWeightsToArray(float* outArr) {
    if (!outArr) return;
    AIWeights w;
    getAIWeights(w);
    outArr[0] = static_cast<float>(w.globalAlphaEarly);
    outArr[1] = static_cast<float>(w.globalAlphaLate);
    outArr[2] = static_cast<float>(w.liveDecayFactor);
    outArr[3] = static_cast<float>(w.tacticalLiveBonus);
    outArr[4] = static_cast<float>(w.parityBonus);
    outArr[5] = static_cast<float>(w.parityPenalty);
    outArr[6] = static_cast<float>(w.adjHitBonus);
    outArr[7] = static_cast<float>(w.adjLineBonus);
    outArr[8] = static_cast<float>(w.diagHitBonus);
    outArr[9] = static_cast<float>(w.fitScoreNearAdjFactor);
    outArr[10] = static_cast<float>(w.fitScoreBaseFactor);
    outArr[11] = static_cast<float>(w.noFitPenalty);
    outArr[12] = static_cast<float>(w.placementHitMultiplier);
    outArr[13] = static_cast<float>(w.mcIterations);
    outArr[14] = static_cast<float>(w.mcBlendRatio);
    outArr[15] = static_cast<float>(w.mcBlendThresholdCells);
}
