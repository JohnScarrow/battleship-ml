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

// Log parsing → counts (removed for WASM - no file I/O)
// void learnFromLog(const string &filename, ...);

// Counts → probabilities
void computeProbabilities(int hitCount[NUM_ROWS][NUM_COLS],
                          int missCount[NUM_ROWS][NUM_COLS],
                          double hitProb[NUM_ROWS][NUM_COLS]);

// Heatmap saver (removed for WASM - no file I/O)
// void saveHeatmap(const string &filename, double hitProb[NUM_ROWS][NUM_COLS]);

void updateLiveHeatmap(const char board[NUM_ROWS][NUM_COLS],
                       double liveProb[NUM_ROWS][NUM_COLS],
                       const int remaining[NUM_SHIPS]);

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
