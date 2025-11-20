#include "Tournament.h"
#include <sstream>
#include <iomanip>
#include <cstring>

static float BOARD_BUFFER[100]; // reused for snapshots
static float HEATMAP_BUFFER[100]; // reused for heatmap snapshots
static float BOARD1_BUFFER[100]; // Player 1's board
static float BOARD2_BUFFER[100]; // Player 2's board
static float HEAT1_BUFFER[100];  // Player 1's heatmap
static float HEAT2_BUFFER[100];  // Player 2's heatmap

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
    std::memset(liveHitsP1, 0, sizeof(liveHitsP1));
    std::memset(liveMissesP1, 0, sizeof(liveMissesP1));
    std::memset(liveHitsP2, 0, sizeof(liveHitsP2));
    std::memset(liveMissesP2, 0, sizeof(liveMissesP2));
    std::memset(hitProb, 0, sizeof(hitProb));
    std::memset(liveProbP1, 0, sizeof(liveProbP1));
    std::memset(liveProbP2, 0, sizeof(liveProbP2));

    // Learn from prior log if available (native runs only; in browser omit file I/O)
    // learnFromLog("battleship.log", hitCount, missCount);

    // Initialize global hit probability using placement enumeration on a blank view
    char initialView[NUM_ROWS][NUM_COLS];
    for (int r = 0; r < NUM_ROWS; ++r)
        for (int c = 0; c < NUM_COLS; ++c)
            initialView[r][c] = '-';
    computePlacementProbabilities(initialView, computerShipSizes, hitProb);

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
        // Choose which liveProb to use depending on which player is choosing
        double (*livePtr)[NUM_COLS] = (turn == 0) ? liveProbP1 : liveProbP2;
        std::tie(row, col) = chooseAIMove(targetBoard, hitProb, livePtr, ts, targetShipSizes, turnCount);
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
        // record observation for the shooter: if turn==0, Player1 observed this hit on Player2
        if (turn == 0) liveHitsP1[row][col]++;
        else liveHitsP2[row][col]++;
    } else {
        currentStats.misses++;
        if (turn == 0) liveMissesP1[row][col]++;
        else liveMissesP2[row][col]++;
    }
    currentStats.totalShots++;
    currentStats.hitMissRatio = currentStats.totalShots ?
        (100.0 * currentStats.hits / currentStats.totalShots) : 0.0;

    // Update both players' heatmaps from their own observations
    // Player1 observes the computer board; Player2 observes the player board.
    char viewP1[NUM_ROWS][NUM_COLS];
    char viewP2[NUM_ROWS][NUM_COLS];
    for (int r = 0; r < NUM_ROWS; ++r) {
        for (int c = 0; c < NUM_COLS; ++c) {
            viewP1[r][c] = (liveHitsP1[r][c] > 0) ? 'X' : (liveMissesP1[r][c] > 0 ? 'm' : '-');
            viewP2[r][c] = (liveHitsP2[r][c] > 0) ? 'X' : (liveMissesP2[r][c] > 0 ? 'm' : '-');
        }
    }
    // Player1's probabilities target the computer's ships
    computePlacementProbabilities(viewP1, computerShipSizes, liveProbP1);
    // Player2's probabilities target the player's ships
    computePlacementProbabilities(viewP2, playerShipSizes, liveProbP2);
    
    // For MC blending, use the current shooter's view and remaining cells
    char view[NUM_ROWS][NUM_COLS];
    int remainingCells = 0;
    for (int i = 0; i < NUM_SHIPS; ++i) remainingCells += targetShipSizes[i];
    for (int r = 0; r < NUM_ROWS; ++r) {
        for (int c = 0; c < NUM_COLS; ++c) {
            if (turn == 0) {
                view[r][c] = viewP1[r][c];
            } else {
                view[r][c] = viewP2[r][c];
            }
        }
    }
    // Endgame Monte-Carlo blend when few ship cells remain
    if (remainingCells <= gAIWeights.mcBlendThresholdCells) {
        double mcMap[NUM_ROWS][NUM_COLS];
        monteCarloProbabilities(view, targetShipSizes, gAIWeights.mcIterations, mcMap);
        for (int r = 0; r < NUM_ROWS; ++r)
            for (int c = 0; c < NUM_COLS; ++c) {
                if (turn == 0) liveProbP1[r][c] = (1.0 - gAIWeights.mcBlendRatio) * liveProbP1[r][c] + gAIWeights.mcBlendRatio * mcMap[r][c];
                else liveProbP2[r][c] = (1.0 - gAIWeights.mcBlendRatio) * liveProbP2[r][c] + gAIWeights.mcBlendRatio * mcMap[r][c];
            }
    }

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
            // Provide the 'global' liveProb for debugging - average of both players' maps
            double val = 0.0;
            val = 0.5 * (liveProbP1[r][c] + liveProbP2[r][c]);
            HEATMAP_BUFFER[r * NUM_COLS + c] = static_cast<float>(val);
        }
    }
    return HEATMAP_BUFFER;
}

const float* RoundState::snapshotPlayer1Board() {
    // Player 1's board (what Player 2 is attacking)
    const char (*b)[NUM_COLS] = playerBoard;
    for (int r = 0; r < NUM_ROWS; ++r) {
        for (int c = 0; c < NUM_COLS; ++c) {
            float v = 0.0f;
            if (b[r][c] == MISS) v = -1.0f;
            else if (b[r][c] == HIT) v = 1.0f;
            BOARD1_BUFFER[r * NUM_COLS + c] = v;
        }
    }
    return BOARD1_BUFFER;
}

const float* RoundState::snapshotPlayer2Board() {
    // Player 2's board (what Player 1 is attacking)
    const char (*b)[NUM_COLS] = computerBoard;
    for (int r = 0; r < NUM_ROWS; ++r) {
        for (int c = 0; c < NUM_COLS; ++c) {
            float v = 0.0f;
            if (b[r][c] == MISS) v = -1.0f;
            else if (b[r][c] == HIT) v = 1.0f;
            BOARD2_BUFFER[r * NUM_COLS + c] = v;
        }
    }
    return BOARD2_BUFFER;
}

const float* RoundState::getPlayer1Heatmap() {
    // P1's targeting heatmap
    for (int r = 0; r < NUM_ROWS; ++r) {
        for (int c = 0; c < NUM_COLS; ++c) {
            HEAT1_BUFFER[r * NUM_COLS + c] = static_cast<float>(liveProbP1[r][c]);
        }
    }
    return HEAT1_BUFFER;
}

const float* RoundState::getPlayer2Heatmap() {
    // P2's targeting heatmap
    for (int r = 0; r < NUM_ROWS; ++r) {
        for (int c = 0; c < NUM_COLS; ++c) {
            HEAT2_BUFFER[r * NUM_COLS + c] = static_cast<float>(liveProbP2[r][c]);
        }
    }
    return HEAT2_BUFFER;
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

const float* Tournament::snapshotPlayer1Board() {
    return current.snapshotPlayer1Board();
}

const float* Tournament::snapshotPlayer2Board() {
    return current.snapshotPlayer2Board();
}

const float* Tournament::getPlayer1Heatmap() {
    return current.getPlayer1Heatmap();
}

const float* Tournament::getPlayer2Heatmap() {
    return current.getPlayer2Heatmap();
}

// Player move handling - allows human to click on board
int RoundState::makePlayerMove(int row, int col) {
    // Only allow if mode=2 (player vs AI), turn=0 (player's turn), not game over, and valid cell
    if (mode != 2 || turn != 0 || gameOver) return 0;
    if (row < 0 || row >= NUM_ROWS || col < 0 || col >= NUM_COLS) return 0;
    if (!checkShotIsAvailable(computerBoard, row, col)) return 0; // already shot

    // Execute the shot on the computer's board
    int res = updateBoard(computerBoard, row, col, computerShipSizes);
    bool sunk = false;

    if (res != -1) {
        sunk = updateShipSize(computerShipSizes, res);
        playerStats.hits++;
        liveHitsP1[row][col]++;
    } else {
        playerStats.misses++;
        liveMissesP1[row][col]++;
    }
    playerStats.totalShots++;

    playerStats.hitMissRatio = playerStats.totalShots ?
        (100.0 * playerStats.hits / playerStats.totalShots) : 0.0;

    // Build view and update live probabilities
    char view[NUM_ROWS][NUM_COLS];
    for (int r = 0; r < NUM_ROWS; ++r) {
        for (int c = 0; c < NUM_COLS; ++c) {
            if (liveHitsP1[r][c] > 0) view[r][c] = 'X';
            else if (liveMissesP1[r][c] > 0) view[r][c] = 'm';
            else view[r][c] = '-';
        }
    }
    computePlacementProbabilities(view, computerShipSizes, liveProbP1);

    // Log
    {
        std::ostringstream oss;
        oss << "Player fires (" << row << "," << col << ")";
        if (res != -1) oss << " -> HIT" << (sunk ? " + SUNK" : "");
        else oss << " -> miss";
        lastLog = oss.str();
    }

    // Update player targeting state
    updateTargetStateAfterResult(p1Target, computerBoard, row, col, res, sunk, computerShipSizes);

    // Check win
    if (isWinner(computerShipSizes)) {
        gameOver = true;
        playerStats.won = true;
        computerStats.won = false;
        lastLog = "Player wins!";
        return res != -1 ? 2 : 1; // 2=hit, 1=miss
    }

    turnCount++;
    turn = 1; // switch to AI

    // Return: 1=miss, 2=hit, 3=sunk ship
    if (sunk) return 3;
    if (res != -1) return 2;
    return 1;
}

int RoundState::isPlayerTurn() const {
    return (mode == 2 && turn == 0 && !gameOver) ? 1 : 0;
}

void RoundState::advanceAITurn() {
    // Force the AI to take a turn (call tick once if it's AI's turn)
    if (mode == 2 && turn == 1 && !gameOver) {
        tick();
    }
}
