#ifndef MLFORAI_H
#define MLFORAI_H

#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <cctype>
#include <iomanip>
#include <climits>
#include <numeric>
#include <cmath>
#include "battleship.h"  // NUM_ROWS, NUM_COLS, TargetState, checkShotIsAvailable

using namespace std;

// Adjustable AI weight parameters
struct AIWeights {
    double globalAlphaEarly = 0.75;
    double globalAlphaLate  = 0.55;
    double liveDecayFactor  = 0.02;
    double tacticalLiveBonus = 0.05;
    double parityBonus       = 0.25;
    double parityPenalty     = -0.15;
    double adjHitBonus       = 0.4;
    double adjLineBonus      = 0.3;
    double diagHitBonus      = 0.2;
    double fitScoreNearAdjFactor = 0.30;
    double fitScoreBaseFactor    = 0.20;
    double noFitPenalty      = -0.5;
    double placementHitMultiplier = 2.0;
    int    mcIterations      = 400;
    double mcBlendRatio      = 0.5;
    int    mcBlendThresholdCells = 6;
};

extern AIWeights gAIWeights;
void setAIWeights(const AIWeights &w);
void getAIWeights(AIWeights &out);

// Log parsing → counts (native builds may provide a stub; WASM has no file I/O)
void learnFromLog(const string &filename,
                  int hitCount[NUM_ROWS][NUM_COLS],
                  int missCount[NUM_ROWS][NUM_COLS]);

// Counts → probabilities
void computeProbabilities(int hitCount[NUM_ROWS][NUM_COLS],
                          int missCount[NUM_ROWS][NUM_COLS],
                          double hitProb[NUM_ROWS][NUM_COLS]);

// Heatmap saver (native stub allowed; WASM has no file I/O)
void saveHeatmap(const string &filename, double hitProb[NUM_ROWS][NUM_COLS]);

void updateLiveHeatmap(const char board[NUM_ROWS][NUM_COLS],
                       double liveProb[NUM_ROWS][NUM_COLS],
                       const int remaining[NUM_SHIPS]);

// Placement-based probability: enumerate all legal placements for remaining ships
// and score unshot cells by how many placements would occupy them.
void computePlacementProbabilities(const char boardView[NUM_ROWS][NUM_COLS],
                                   const int remaining[NUM_SHIPS],
                                   double outProb[NUM_ROWS][NUM_COLS]);

// Monte-Carlo sampling fallback (optional) - sample many random legal placements
// and accumulate cell frequencies. Not used by default, but available for experiments.
void monteCarloProbabilities(const char boardView[NUM_ROWS][NUM_COLS],
                             const int remaining[NUM_SHIPS],
                             int iterations,
                             double outProb[NUM_ROWS][NUM_COLS]);

// Heatmap-driven AI (search mode)
pair<int,int> getSmartMove(const char board[NUM_ROWS][NUM_COLS],
                           double hitProb[NUM_ROWS][NUM_COLS]);

// Biased placement helpers
void generatePlacementWeights(double weights[NUM_ROWS][NUM_COLS]);
pair<int,int> pickWeightedCell(double weights[NUM_ROWS][NUM_COLS]);

// Combined search + target mode AI
bool isCellAvailable(const char board[NUM_ROWS][NUM_COLS], int r, int c);
void enqueueNeighbors(TargetState &ts, const char board[NUM_ROWS][NUM_COLS], int r, int c);
void enqueueOrientedLine(TargetState &ts, const char board[NUM_ROWS][NUM_COLS]);

std::pair<int,int> chooseAIMove(const char board[NUM_ROWS][NUM_COLS],
                                double globalProb[NUM_ROWS][NUM_COLS],
                                double liveProb[NUM_ROWS][NUM_COLS],
                                TargetState &ts,
                                const int remaining[NUM_SHIPS],
                                int turn);


double scoreCell(int r, int c,
                 const char board[NUM_ROWS][NUM_COLS],
                 double globalProb[NUM_ROWS][NUM_COLS],
                 double liveProb[NUM_ROWS][NUM_COLS],
                 const int remaining[NUM_SHIPS],
                 int turn);

bool shipFitsAt(const char board[NUM_ROWS][NUM_COLS], int r, int c, int size, bool horiz);

int shipFitScoreAt(const char board[NUM_ROWS][NUM_COLS], int r, int c, const int remaining[NUM_SHIPS]);

double shipFitBiasScoreAt(const char board[NUM_ROWS][NUM_COLS],
                          int r, int c,
                          const int remaining[NUM_SHIPS]);


int countConsecutiveHits(const char board[NUM_ROWS][NUM_COLS],
                         int row, int col, int orientation);

void updateTargetStateAfterResult(TargetState &ts,
                                  const char targetBoard[NUM_ROWS][NUM_COLS],
                                  int shotRow, int shotCol,
                                  int resultShipIndex,
                                  bool sunk,
                                  int targetShipSizes[NUM_SHIPS]);

#endif
