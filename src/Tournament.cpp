#include "Tournament.h"
#include <sstream>
#include <iomanip>
#include <cstring>

static float BOARD_BUFFER[100]; // reused for snapshots
static float HEATMAP_BUFFER[100]; // reused for heatmap snapshots

void RoundState::reset(int mode_, int round_) {
    mode = mode_;
    roundIndex = round_;
    lastLog.clear();

    // Player types
    PlayerType player1Type, player2Type;
    if (mode == 1) { player1Type = HUMAN; player2Type = HUMAN; }
    else if (mode == 2) { player1Type = HUMAN; player2Type = COMPUTER; }
    else { player1Type = COMPUTER; player2Type = COMPUTER; }

    // Boards
    initializeBoard(playerBoard);
    initializeBoard(computerBoard);

    // Placement
    biasedPlaceShipsOnBoard(playerBoard);
    biasedPlaceShipsOnBoard(computerBoard);

    // Ship health
    for (int i = 0; i < NUM_SHIPS; ++i) {
        playerShipSizes[i] = SHIP_SIZES[i];
        computerShipSizes[i] = SHIP_SIZES[i];
    }

    // Stats reset
    playerStats = Stats{};
    computerStats = Stats{};

    // Learning arrays reset
    std::memset(hitCount, 0, sizeof(hitCount));
    std::memset(missCount, 0, sizeof(missCount));
    std::memset(liveHits, 0, sizeof(liveHits));
    std::memset(liveMisses, 0, sizeof(liveMisses));
    std::memset(hitProb, 0, sizeof(hitProb));
    std::memset(liveProb, 0, sizeof(liveProb));

    // Learn from prior log if available (native runs only; in browser omit file I/O)
    // learnFromLog("battleship.log", hitCount, missCount);

    computeProbabilities(hitCount, missCount, hitProb);

    // Who starts
    turn = selectWhoStartsFirst();
    gameOver = false;
    turnCount = 0;

    p1Target = TargetState{};
    p2Target = TargetState{};
}

const char* RoundState::tick() {
    if (gameOver) { lastLog = "[Round already finished]"; return lastLog.c_str(); }

    PlayerType player1Type = (mode == 1 ? HUMAN : (mode == 2 ? HUMAN : COMPUTER));
    PlayerType player2Type = (mode == 1 ? HUMAN : COMPUTER);

    PlayerType currentType = (turn == 0 ? player1Type : player2Type);
    char (*targetBoard)[NUM_COLS] = (turn == 0 ? computerBoard : playerBoard);
    int *targetShipSizes = (turn == 0 ? computerShipSizes : playerShipSizes);
    Stats &currentStats = (turn == 0 ? playerStats : computerStats);
    std::string currentName = (turn == 0 ? "Player1" : "Player2");

    int row = -1, col = -1;
    if (currentType == HUMAN) {
        // In browser, we don’t block for input; leave a message and skip
        lastLog = "[Human move requested; demo runs AI only]";
        // You can later expose feedLine() and parse commands if you want interactive play
        currentType = COMPUTER;
    }

    if (currentType == COMPUTER) {
        TargetState &ts = (turn == 0 ? p1Target : p2Target);
        std::tie(row, col) = chooseAIMove(targetBoard, hitProb, liveProb, ts, targetShipSizes, turnCount);
        if (!checkShotIsAvailable(targetBoard, row, col)) {
            ts.active = false; ts.oriented = false; ts.orientation = 0; ts.queue.clear();
            std::tie(row, col) = getSmartMove(targetBoard, hitProb);
        }
    }

    if (!checkShotIsAvailable(targetBoard, row, col)) {
        // skip if invalid
        turn = 1 - turn;
        lastLog = "[Skipped invalid shot]";
        return lastLog.c_str();
    }

    int res = updateBoard(targetBoard, row, col, targetShipSizes);
    bool sunk = false;
    if (res != -1) {
        sunk = updateShipSize(targetShipSizes, res);
        currentStats.hits++;
        liveHits[row][col]++;
    } else {
        currentStats.misses++;
        liveMisses[row][col]++;
    }
    currentStats.totalShots++;
    currentStats.hitMissRatio = currentStats.totalShots ?
        (100.0 * currentStats.hits / currentStats.totalShots) : 0.0;

    computeProbabilities(liveHits, liveMisses, liveProb);

    // Log message
    {
        std::ostringstream oss;
        oss << currentName << " fires (" << row << "," << col << ")";
        if (res != -1) oss << " -> HIT" << (sunk ? " + SUNK" : "");
        else oss << " -> miss";
        lastLog = oss.str();
    }

    if (currentType == COMPUTER) {
        TargetState &ts = (turn == 0 ? p1Target : p2Target);
        updateTargetStateAfterResult(ts, targetBoard, row, col, res, sunk, targetShipSizes);
    }

    if (isWinner(targetShipSizes)) {
        gameOver = true;
        currentStats.won = true;
        (turn == 0 ? computerStats : playerStats).won = false;
        std::ostringstream oss;
        oss << (turn == 0 ? "Player1" : "Player2") << " wins round " << roundIndex << "!";
        lastLog = oss.str();
        return lastLog.c_str();
    }

    turnCount++;
    turn = 1 - turn;
    return lastLog.c_str();
}

const float* RoundState::snapshotBoard(bool showComputerBoard) {
    // For demo, show the computer’s board perspective (hits/misses inflicted)
    const char (*b)[NUM_COLS] = showComputerBoard ? computerBoard : playerBoard;
    // Map chars to floats: empty 0, miss -1, hit 1
    for (int r = 0; r < NUM_ROWS; ++r) {
        for (int c = 0; c < NUM_COLS; ++c) {
            float v = 0.0f;
            if (b[r][c] == MISS) v = -1.0f;
            else if (b[r][c] == HIT) v = 1.0f;
            // leave ships/unknown as 0 for clean visualization
            BOARD_BUFFER[r * NUM_COLS + c] = v;
        }
    }
    return BOARD_BUFFER;
}

const float* RoundState::getHeatmapSnapshot() {
    // Return the live probability heatmap (0.0-1.0)
    for (int r = 0; r < NUM_ROWS; ++r) {
        for (int c = 0; c < NUM_COLS; ++c) {
            HEATMAP_BUFFER[r * NUM_COLS + c] = static_cast<float>(liveProb[r][c]);
        }
    }
    return HEATMAP_BUFFER;
}

void Tournament::start(int mode, int n) {
    totalRounds = n;
    currentRoundIdx = 0;
    p1WinsAccum = p2WinsAccum = 0;
    shotsP1Accum = shotsP1Accum = 0;
    shotsP2Accum = shotsP2Accum = 0;
    current.reset(mode, 1);
}

const char* Tournament::tick() {
    if (currentRoundIdx >= totalRounds && current.isFinished()) {
        static std::string doneMsg = "[Tournament finished]";
        return doneMsg.c_str();
    }

    const char* msg = current.tick();

    if (current.isFinished()) {
        p1WinsAccum += current.winnerP1();
        p2WinsAccum += current.winnerP2();
        shotsP1Accum += current.playerStats.totalShots;
        shotsP2Accum += current.computerStats.totalShots;
        currentRoundIdx++;
        if (currentRoundIdx < totalRounds) {
            current.reset(current.mode, currentRoundIdx + 1);
            // Announce new game
            static std::string startMsg;
            startMsg = "[New game started: #" + std::to_string(currentRoundIdx + 1) + "]";
            return startMsg.c_str();
        } else {
            static std::string finalMsg;
            std::ostringstream oss;
            oss << "[Tournament complete] P1 wins: " << p1WinsAccum
                << " | P2 wins: " << p2WinsAccum
                << " | P1 avg shots: "
                << std::fixed << std::setprecision(2)
                << (totalRounds ? double(shotsP1Accum)/totalRounds : 0.0)
                << " | P2 avg shots: "
                << (totalRounds ? double(shotsP2Accum)/totalRounds : 0.0);
            finalMsg = oss.str();
            return finalMsg.c_str();
        }
    }
    return msg;
}

int Tournament::done() const {
    return (currentRoundIdx >= totalRounds && current.isFinished()) ? 1 : 0;
}

const float* Tournament::snapshotBoard() {
    return current.snapshotBoard(true);
}

const float* Tournament::getHeatmapSnapshot() {
    return current.getHeatmapSnapshot();
}
