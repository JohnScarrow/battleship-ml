#pragma once
#include <string>
#include <vector>
#include <queue>
#include "battleship.h"
#include "MLforAI.h"

enum class GamePhase { Init, PlayerTurn, AITurn, Finished };

struct RoundState {
    // Config
    int mode = 3;          // 1 PvP, 2 PvC, 3 CvC
    int roundIndex = 1;

    // Boards
    char playerBoard[NUM_ROWS][NUM_COLS];
    char computerBoard[NUM_ROWS][NUM_COLS];

    // Ship health
    int playerShipSizes[NUM_SHIPS];
    int computerShipSizes[NUM_SHIPS];

    // Stats
    Stats playerStats{}, computerStats{};

    // Learning arrays
    int hitCount[NUM_ROWS][NUM_COLS] = {0};
    int missCount[NUM_ROWS][NUM_COLS] = {0};
    double hitProb[NUM_ROWS][NUM_COLS] = {0};
    // Per-player live observation arrays: what each player has observed of the opponent
    int liveHitsP1[NUM_ROWS][NUM_COLS] = {0};
    int liveMissesP1[NUM_ROWS][NUM_COLS] = {0};
    double liveProbP1[NUM_ROWS][NUM_COLS] = {0};

    int liveHitsP2[NUM_ROWS][NUM_COLS] = {0};
    int liveMissesP2[NUM_ROWS][NUM_COLS] = {0};
    double liveProbP2[NUM_ROWS][NUM_COLS] = {0};

    // Targeting states
    TargetState p1Target{}, p2Target{};

    // Control
    int turn = 0;              // 0 -> Player1, 1 -> Player2
    int turnCount = 0;
    bool gameOver = false;

    // Scratch log buffer (returned per tick)
    std::string lastLog;

    void reset(int mode_, int round_);
    // Advances one logical step; returns short log
    const char* tick();
    // Board snapshot for JS (100 floats: 0 empty, 1 hit, -1 miss, optional >1 ship id)
    const float* snapshotBoard(bool showComputerBoard = true);
    const float* snapshotPlayer1Board(); // Player 1's board (what P2 is attacking)
    const float* snapshotPlayer2Board(); // Player 2's board (what P1 is attacking)
    // Heatmap snapshot for JS (100 floats: 0.0-1.0 probabilities)
    const float* getHeatmapSnapshot();
    const float* getPlayer1Heatmap(); // P1's targeting heatmap
    const float* getPlayer2Heatmap(); // P2's targeting heatmap
    bool isFinished() const { return gameOver; }
    int makePlayerMove(int row, int col);
    int isPlayerTurn() const;
    void advanceAITurn();
    int winnerP1() const { return playerStats.won ? 1 : 0; }
    int winnerP2() const { return computerStats.won ? 1 : 0; }
};

// Tournament controller
struct Tournament {
    int totalRounds = 10;
    int currentRoundIdx = 0;
    RoundState current;
    long long shotsP1Accum = 0;
    long long shotsP2Accum = 0;
    int p1WinsAccum = 0;
    int p2WinsAccum = 0;

    void start(int mode, int n);
    const char* tick();
    int done() const;
    const float* snapshotBoard();
    const float* snapshotPlayer1Board();
    const float* snapshotPlayer2Board();
    const float* getHeatmapSnapshot();
    const float* getPlayer1Heatmap();
    const float* getPlayer2Heatmap();
};
