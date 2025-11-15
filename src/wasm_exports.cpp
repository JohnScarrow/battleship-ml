#include "wasm_exports.h"
#include "Tournament.h"
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

}
