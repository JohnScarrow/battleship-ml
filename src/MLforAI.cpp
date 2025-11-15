#include "MLforAI.h"
#include "battleship.h"

using namespace std;

// Global AI weights instance
AIWeights gAIWeights{};

void setAIWeights(const AIWeights &w) { gAIWeights = w; }
void getAIWeights(AIWeights &out) { out = gAIWeights; }

// learnFromLog removed for WASM - no file I/O
// Learning happens in-memory during the tournament

/**
 * @brief Compute the hit probability for each cell on the board
 *
 * This function takes in the hit and miss counts for each cell on the board
 * and computes the hit probability for each cell. The hit probability is
 * computed as the number of hits divided by the total number of shots
 * (hits + misses) plus one. The plus one is to avoid dividing by zero.
 *
 * @param hitCount The 2D array to store hit counts
 * @param missCount The 2D array to store miss counts
 * @param hitProb The 2D array to store hit probabilities
 */
void computeProbabilities(int hitCount[NUM_ROWS][NUM_COLS],
                          int missCount[NUM_ROWS][NUM_COLS],
                          double hitProb[NUM_ROWS][NUM_COLS]) {
    for (int r = 0; r < NUM_ROWS; r++) {
        for (int c = 0; c < NUM_COLS; c++) {
            int h = hitCount[r][c];
            int m = missCount[r][c];
            hitProb[r][c] = (double)h / (h + m + 1); // +1 avoids divide by zero
        }
    }
}

/**
 * @brief Gets the smart move by searching for the cell with the highest hit probability
 *
 * This function takes in the current state of the board and the hit probabilities
 * for each cell and returns the coordinates of the cell with the highest hit
 * probability. This is the smart move that the AI should make.
 *
 * @param board The current state of the board
 * @param hitProb The hit probabilities for each cell on the board
 * @return The coordinates of the cell with the highest hit probability
 */
pair<int,int> getSmartMove(const char board[NUM_ROWS][NUM_COLS],
                           double hitProb[NUM_ROWS][NUM_COLS]) {
    int bestRow = -1, bestCol = -1;
    double bestScore = -1.0;

    for (int r = 0; r < NUM_ROWS; r++) {
        for (int c = 0; c < NUM_COLS; c++) {
            if (checkShotIsAvailable(board, r, c)) {
                if (hitProb[r][c] > bestScore) {
                    bestScore = hitProb[r][c];
                    bestRow = r;
                    bestCol = c;
                }
            }
        }
    }
    return {bestRow, bestCol};
}

// saveHeatmap removed for WASM - no file I/O
// Heatmap data is exposed via getHeatmapSnapshot()

// Native stub: read historical log (no-op in this build)
void learnFromLog(const string &filename,
                  int hitCount[NUM_ROWS][NUM_COLS],
                  int missCount[NUM_ROWS][NUM_COLS]) {
    // Intentionally left blank for WASM/no-file environments; native runs may implement this.
    (void)filename; (void)hitCount; (void)missCount;
}

// Native stub: save heatmap to file (no-op)
void saveHeatmap(const string &filename, double hitProb[NUM_ROWS][NUM_COLS]) {
    (void)filename; (void)hitProb;
}


void updateLiveHeatmap(const char board[NUM_ROWS][NUM_COLS],
                       double liveProb[NUM_ROWS][NUM_COLS],
                       const int remaining[NUM_SHIPS]) {
    // Reset liveProb
    for (int r = 0; r < NUM_ROWS; ++r)
        for (int c = 0; c < NUM_COLS; ++c)
            liveProb[r][c] = 0.0;

    for (int r = 0; r < NUM_ROWS; ++r) {
        for (int c = 0; c < NUM_COLS; ++c) {
            if (board[r][c] != 'X') continue;

            for (int i = 0; i < NUM_SHIPS; ++i) {
                if (remaining[i] == 0) continue;
                int len = remaining[i];

                for (int horiz = 0; horiz <= 1; ++horiz) {
                    int dr = horiz ? 0 : 1;
                    int dc = horiz ? 1 : 0;

                    for (int offset = 0; offset < len; ++offset) {
                        int startR = r - offset * dr;
                        int startC = c - offset * dc;

                        bool valid = true;
                        for (int k = 0; k < len; ++k) {
                            int nr = startR + k * dr;
                            int nc = startC + k * dc;
                            if (nr < 0 || nr >= NUM_ROWS || nc < 0 || nc >= NUM_COLS)
                                { valid = false; break; }
                            if (board[nr][nc] == 'm') { valid = false; break; }
                        }

                        if (valid) {
                            for (int k = 0; k < len; ++k) {
                                int nr = startR + k * dr;
                                int nc = startC + k * dc;
                                if (board[nr][nc] == '-')  // only unshot cells
                                    liveProb[nr][nc] += 1.0;
                            }
                        }
                    }
                }
            }
        }
    }

    // Normalize liveProb
    double maxVal = 0.0;
    for (int r = 0; r < NUM_ROWS; ++r)
        for (int c = 0; c < NUM_COLS; ++c)
            if (liveProb[r][c] > maxVal) maxVal = liveProb[r][c];

    if (maxVal > 0.0) {
        for (int r = 0; r < NUM_ROWS; ++r)
            for (int c = 0; c < NUM_COLS; ++c)
                liveProb[r][c] /= maxVal;
    }
}






// Check if a cell is within bounds and still available
bool isCellAvailable(const char board[NUM_ROWS][NUM_COLS], int r, int c) {
    return r >= 0 && r < NUM_ROWS && c >= 0 && c < NUM_COLS
           && checkShotIsAvailable(board, r, c);
}

// Queue up neighbors (up, down, left, right) after a hit
static bool inQueue(const TargetState &ts, int r, int c) {
    for (auto &p : ts.queue) if (p.first == r && p.second == c) return true;
    return false;
}

void enqueueNeighbors(TargetState &ts, const char board[NUM_ROWS][NUM_COLS], int r, int c) {
    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};
    for (int k = 0; k < 4; ++k) {
        int nr = r + dr[k], nc = c + dc[k];
        if (isCellAvailable(board, nr, nc)) {
            if (!inQueue(ts, nr, nc)) ts.queue.emplace_back(nr, nc);
        }
    }
}

// Extend in a line along the current orientation, both directions
void enqueueOrientedLine(TargetState &ts,
                         const char targetBoard[NUM_ROWS][NUM_COLS]) {
    int r = ts.lastHitRow;
    int c = ts.lastHitCol;

    // Preserve existing queue; we'll append oriented extensions uniquely

    if (ts.orientation == 1) { // horizontal
        // left
        for (int cc = c - 1; cc >= 0; --cc) {
            if (checkShotIsAvailable(targetBoard, r, cc)) {
                if (!inQueue(ts, r, cc)) ts.queue.push_back({r, cc});
            } else if (targetBoard[r][cc] != 'X') break;
        }
        // right
        for (int cc = c + 1; cc < NUM_COLS; ++cc) {
            if (checkShotIsAvailable(targetBoard, r, cc)) {
                if (!inQueue(ts, r, cc)) ts.queue.push_back({r, cc});
            } else if (targetBoard[r][cc] != 'X') break;
        }
    } else if (ts.orientation == 2) { // vertical
        // up
        for (int rr = r - 1; rr >= 0; --rr) {
            if (checkShotIsAvailable(targetBoard, rr, c)) {
                if (!inQueue(ts, rr, c)) ts.queue.push_back({rr, c});
            } else if (targetBoard[rr][c] != 'X') break;
        }
        // down
        for (int rr = r + 1; rr < NUM_ROWS; ++rr) {
            if (checkShotIsAvailable(targetBoard, rr, c)) {
                if (!inQueue(ts, rr, c)) ts.queue.push_back({rr, c});
            } else if (targetBoard[rr][c] != 'X') break;
        }
    }
}

// AI move selector using parity + heatmap (search) and weighted target mode
std::pair<int,int> chooseAIMove(const char board[NUM_ROWS][NUM_COLS],
                                double globalProb[NUM_ROWS][NUM_COLS],
                                double liveProb[NUM_ROWS][NUM_COLS],
                                TargetState &ts,
                                const int remaining[NUM_SHIPS],
                                int turn) {

    updateLiveHeatmap(board, liveProb, remaining);


    // --- Target mode ---
    if (ts.active && !ts.queue.empty()) {
        pair<int,int> best = {-1,-1};
        double bestScore = -1.0;
        for (auto &mv : ts.queue) {
            int r = mv.first, c = mv.second;
            if (!checkShotIsAvailable(board, r, c)) continue;
            double s = scoreCell(r, c, board, globalProb, liveProb, remaining, turn);
            if (s > bestScore) {
                bestScore = s;
                best = mv;
            }
        }
        // remove chosen cell from queue (if any) and return
        if (best.first != -1) {
            // remove chosen element only
            auto it = std::remove_if(ts.queue.begin(), ts.queue.end(), [&](auto &p){return p.first==best.first && p.second==best.second;});
            ts.queue.erase(it, ts.queue.end());
            return best;
        }
        // If queue was invalid, reset targeting
        ts.active = false; ts.oriented = false; ts.orientation = 0;
    }

    // --- Search mode: hybrid scoring ---
    double bestScore = -1.0;
    pair<int,int> bestMove = {-1,-1};

    // Iterate cells in a center-out spiral so ties and small score differences bias toward center
    int centerR = NUM_ROWS / 2 - (NUM_ROWS % 2 == 0 ? 1 : 0);
    int centerC = NUM_COLS / 2 - (NUM_COLS % 2 == 0 ? 1 : 0);
    int layer = 0;
    int found = 0;
    while (layer <= max(NUM_ROWS, NUM_COLS) && found < NUM_ROWS * NUM_COLS) {
        for (int dr = -layer; dr <= layer; ++dr) {
            for (int dc = -layer; dc <= layer; ++dc) {
                if (abs(dr) != layer && abs(dc) != layer) continue;
                int r = centerR + dr;
                int c = centerC + dc;
                if (r < 0 || r >= NUM_ROWS || c < 0 || c >= NUM_COLS) continue;
                found++;
                double s = scoreCell(r, c, board, globalProb, liveProb, remaining, turn);
                if (s > bestScore) {
                    bestScore = s;
                    bestMove = {r, c};
                }
            }
        }
        layer++;
    }

    // Fallback if nothing valid
    if (bestMove.first == -1) {
        for (int r = 0; r < NUM_ROWS; ++r)
            for (int c = 0; c < NUM_COLS; ++c)
                if (checkShotIsAvailable(board, r, c))
                    return {r, c};
    }
    return bestMove;


    // Absolute last resort: return (0,0) to avoid (-1,-1)
    return {0,0};
}


// Hybrid scoring: blend heatmap, parity, and adjacency bonuses
double scoreCell(int r, int c,
                 const char board[NUM_ROWS][NUM_COLS],
                 double globalProb[NUM_ROWS][NUM_COLS],
                 double liveProb[NUM_ROWS][NUM_COLS],
                 const int remaining[NUM_SHIPS],
                 int turn) {
    
    
    double score = 0.0;

    if (!checkShotIsAvailable(board, r, c)) return -1.0;

    double fitScore = shipFitBiasScoreAt(board, r, c, remaining);

    // Only apply fit bias if heatmap is promising
    // if (globalProb[r][c] + liveProb[r][c] > 0.2) {
    //     score += 0.05 * fitScore;  // reduced influence
    // }

    
        // Ship-length-aware pruning
    int minShipSize = INT_MAX;
    for (int i = 0; i < NUM_SHIPS; ++i)
        if (remaining[i] > 0 && remaining[i] < minShipSize)
            minShipSize = remaining[i];

    bool canFit = false;
    for (int horiz = 0; horiz <= 1 && !canFit; ++horiz)
        canFit = shipFitsAt(board, r, c, minShipSize, horiz);
    if (!canFit) score += gAIWeights.noFitPenalty; // Penalize cells that can't fit smallest ship

    
    // Dynamic heatmap weighting with smoother decay and stronger tactical bias
    double alpha = (turn < 10) ? gAIWeights.globalAlphaEarly : gAIWeights.globalAlphaLate; // Global
    double beta = 1.0 - alpha; // Live
    double decay = exp(-gAIWeights.liveDecayFactor * turn);  // smoother fade over time

    score += alpha * globalProb[r][c];
    score += beta * liveProb[r][c] * decay;

    // Tactical streak bonus
    if (liveProb[r][c] > 0.0) score += gAIWeights.tacticalLiveBonus;


    bool bigShipLeft = false;
    for (int i = 0; i < NUM_SHIPS; ++i) if (remaining[i] >= 3) { bigShipLeft = true; break; }
    if (bigShipLeft) {
        if ((r + c) % 2 == 0) score += gAIWeights.parityBonus;
        else score += gAIWeights.parityPenalty; }


// Cardinal Adjacency Loop
    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};
    int adjHits = 0;
    for (int k = 0; k < 4; ++k) {
        int nr = r + dr[k], nc = c + dc[k];
        if (nr >= 0 && nr < NUM_ROWS && nc >= 0 && nc < NUM_COLS) {
            if (board[nr][nc] == 'X') {
                adjHits++;
                score += gAIWeights.adjHitBonus;

                // Bonus for extending in same direction
                int nnr = nr + dr[k], nnc = nc + dc[k];
                if (nnr >= 0 && nnr < NUM_ROWS && nnc >= 0 && nnc < NUM_COLS) {
                    if (board[nnr][nnc] == 'X') score += gAIWeights.adjLineBonus;
                }
            }
        }
    }

    const int dr_diag[4] = {-1, -1, 1, 1};
const int dc_diag[4] = {-1, 1, -1, 1};
for (int k = 0; k < 4; ++k) {
    int nr = r + dr_diag[k], nc = c + dc_diag[k];
    if (nr >= 0 && nr < NUM_ROWS && nc >= 0 && nc < NUM_COLS) {
        if (board[nr][nc] == 'X') score += gAIWeights.diagHitBonus;
    }
}



    if (adjHits > 2) {
        double fitScore = shipFitBiasScoreAt(board, r, c, remaining);
        score += gAIWeights.fitScoreNearAdjFactor * fitScore;  // Only boost fit if near a hit
    }

    score += adjHits * (gAIWeights.adjHitBonus + 0.2); // slight compounded effect
    score += gAIWeights.fitScoreBaseFactor * fitScore; // base fit influence
    
    // Debug output
    // printf("Turn %d | Global: %.3f | Live: %.3f | Decay: %.3f | Weighted Live: %.3f\n",
    //    turn, globalProb[r][c], liveProb[r][c], decay, beta * liveProb[r][c] * decay);


    return score;
}

bool shipFitsAt(const char board[NUM_ROWS][NUM_COLS], int r, int c, int size, bool horiz) {
    if (horiz) {
        if (c + size > NUM_COLS) return false;
        for (int k = 0; k < size; ++k) {
            char cell = board[r][c + k];
            if (cell != '-' && !isShipSymbol(cell) && cell != 'X') return false;
        }
        return true;
    } else {
        if (r + size > NUM_ROWS) return false;
        for (int k = 0; k < size; ++k) {
            char cell = board[r + k][c];
            if (cell != '-' && !isShipSymbol(cell) && cell != 'X') return false;
        }
        return true;
    }
}

int shipFitScoreAt(const char board[NUM_ROWS][NUM_COLS],
                   int r, int c,
                   const int remaining[NUM_SHIPS]) {
    int score = 0;
    for (int i = 0; i < NUM_SHIPS; ++i) {
        int size = remaining[i];
        if (size <= 0) continue;
        for (int startC = c - size + 1; startC <= c; ++startC) {
            if (startC < 0 || startC + size > NUM_COLS) continue;
            if (shipFitsAt(board, r, startC, size, true)) score++;
        }
        for (int startR = r - size + 1; startR <= r; ++startR) {
            if (startR < 0 || startR + size > NUM_ROWS) continue;
            if (shipFitsAt(board, startR, c, size, false)) score++;
        }
    }
    return score;
}

double shipFitBiasScoreAt(const char board[NUM_ROWS][NUM_COLS],
                          int r, int c,
                          const int remaining[NUM_SHIPS]) {
    double score = 0.0;
    int activeShips = 0;

    for (int i = 0; i < NUM_SHIPS; ++i) {
        if (remaining[i] == 0) continue;
        activeShips++;

        int len = remaining[i];
        double weight = 1.0 + 0.2 * len;  // gentle bias toward longer ships

        for (int horiz = 0; horiz <= 1; ++horiz) {
            if (shipFitsAt(board, r, c, len, horiz)) {
                score += weight;
            }
        }
    }

    if (activeShips > 0) score /= activeShips;

    // Cap the score to prevent runaway values
    if (score > 3.0) score = 3.0;

    return score;
}






// Count consecutive hits in a given orientation (horizontal or vertical)
int countConsecutiveHits(const char board[NUM_ROWS][NUM_COLS],
                         int row, int col, int orientation) {
    int count = 1;
    if (orientation == 1) { // horizontal
        for (int c = col - 1; c >= 0 && board[row][c] == 'X'; --c) count++;
        for (int c = col + 1; c < NUM_COLS && board[row][c] == 'X'; ++c) count++;
    } else if (orientation == 2) { // vertical
        for (int r = row - 1; r >= 0 && board[r][col] == 'X'; --r) count++;
        for (int r = row + 1; r < NUM_ROWS && board[r][col] == 'X'; ++r) count++;
    }
    return count;
}

// Update target state after applying a shot result
void updateTargetStateAfterResult(TargetState &ts,
                                  const char targetBoard[NUM_ROWS][NUM_COLS],
                                  int shotRow, int shotCol,
                                  int resultShipIndex,
                                  bool sunk,
                                  int targetShipSizes[NUM_SHIPS]) {
    if (resultShipIndex != -1) { // HIT
        if (!ts.active) {
            ts.active = true;
            ts.lastHitRow = shotRow;
            ts.lastHitCol = shotCol;
            ts.oriented = false;
            ts.orientation = 0;
            ts.queue.clear();
            enqueueNeighbors(ts, targetBoard, shotRow, shotCol);
        } else {
            // Determine orientation if not set
            if (!ts.oriented) {
                if (shotRow == ts.lastHitRow && shotCol != ts.lastHitCol) {
                    ts.oriented = true; ts.orientation = 1; // horizontal
                } else if (shotCol == ts.lastHitCol && shotRow != ts.lastHitRow) {
                    ts.oriented = true; ts.orientation = 2; // vertical
                }
            }
            ts.lastHitRow = shotRow;
            ts.lastHitCol = shotCol;

            if (ts.oriented) {
                // Extend along the line and reverse if blocked
                enqueueOrientedLine(ts, targetBoard);

                // Ship-length awareness (optional thresholding)
                int streak = countConsecutiveHits(targetBoard, shotRow, shotCol, ts.orientation);
                int minRemaining = INT_MAX;
                for (int i = 0; i < NUM_SHIPS; ++i) {
                    if (targetShipSizes[i] > 0 && targetShipSizes[i] < minRemaining)
                        minRemaining = targetShipSizes[i];
                }
                // We always keep extending; minRemaining can guide future pruning if you want
            } else {
                ts.queue.clear();
                enqueueNeighbors(ts, targetBoard, shotRow, shotCol);
            }
        }

        if (sunk) {
            // Reset targeting when a ship is sunk
            ts.active = false;
            ts.oriented = false;
            ts.orientation = 0;
            ts.queue.clear();
        }
    } else { // MISS
        if (ts.active && ts.oriented) {
            // Reverse direction: rebuild line queue from last known hit
            ts.queue.clear();
            enqueueOrientedLine(ts, targetBoard);
        } else if (ts.active && ts.queue.empty()) {
            ts.active = false;
            ts.oriented = false;
            ts.orientation = 0;
        }
    }
}

// Generate a weight for each cell: higher in the center, lower at edges
void generatePlacementWeights(double weights[NUM_ROWS][NUM_COLS]) {
    for (int r = 0; r < NUM_ROWS; ++r) {
        for (int c = 0; c < NUM_COLS; ++c) {
            // Distance from edge (favor center)
            int dr = min(r, NUM_ROWS - 1 - r);
            int dc = min(c, NUM_COLS - 1 - c);
            int edgeDist = min(dr, dc);
            weights[r][c] = 1.0 + edgeDist * 0.5;
        }
    }
}


// Enumerate placements for each remaining ship and count how many placements
// cover each unknown cell. Produces a normalized probability map in outProb.
void computePlacementProbabilities(const char boardView[NUM_ROWS][NUM_COLS],
                                   const int remaining[NUM_SHIPS],
                                   double outProb[NUM_ROWS][NUM_COLS]) {
    // zero counts
    int counts[NUM_ROWS][NUM_COLS] = {0};
    for (int r = 0; r < NUM_ROWS; ++r)
        for (int c = 0; c < NUM_COLS; ++c)
            outProb[r][c] = 0.0;

    // For each ship size remaining, enumerate placements
    for (int i = 0; i < NUM_SHIPS; ++i) {
        int len = remaining[i];
        if (len <= 0) continue;

        for (int horiz = 0; horiz <= 1; ++horiz) {
            int dr = horiz ? 0 : 1;
            int dc = horiz ? 1 : 0;

            for (int r = 0; r < NUM_ROWS; ++r) {
                for (int c = 0; c < NUM_COLS; ++c) {
                    int endR = r + dr * (len - 1);
                    int endC = c + dc * (len - 1);
                    if (endR < 0 || endR >= NUM_ROWS || endC < 0 || endC >= NUM_COLS) continue;

                    bool valid = true;
                    for (int k = 0; k < len; ++k) {
                        int nr = r + k * dr;
                        int nc = c + k * dc;
                        char cell = boardView[nr][nc];
                        if (cell == 'm') { valid = false; break; } // placement hits a known miss
                        // 'X' (known hit) is allowed and desirable
                    }
                    if (!valid) continue;

                    // Determine placement weight: placements that cover existing hits are more valuable
                    int coversHit = 0;
                    for (int k = 0; k < len; ++k) {
                        int nr = r + k * dr;
                        int nc = c + k * dc;
                        if (boardView[nr][nc] == 'X') coversHit++;
                    }
                    double placementWeight = 1.0 + gAIWeights.placementHitMultiplier * coversHit; // increase weight if it includes hits

                    // This placement is valid; increment counts for unknown cells
                    for (int k = 0; k < len; ++k) {
                        int nr = r + k * dr;
                        int nc = c + k * dc;
                        if (boardView[nr][nc] == '-') counts[nr][nc] += static_cast<int>(placementWeight);
                    }
                }
            }
        }
    }

    // Find max count for normalization
    int maxCount = 0;
    for (int r = 0; r < NUM_ROWS; ++r)
        for (int c = 0; c < NUM_COLS; ++c)
            if (counts[r][c] > maxCount) maxCount = counts[r][c];

    if (maxCount == 0) {
        // fallback: small uniform map for any available shots
        for (int r = 0; r < NUM_ROWS; ++r)
            for (int c = 0; c < NUM_COLS; ++c)
                outProb[r][c] = (boardView[r][c] == '-') ? 1.0 : 0.0;
        // normalize
        double localMax = 0.0;
        for (int r = 0; r < NUM_ROWS; ++r)
            for (int c = 0; c < NUM_COLS; ++c)
                if (outProb[r][c] > localMax) localMax = outProb[r][c];
        if (localMax > 0.0) for (int r = 0; r < NUM_ROWS; ++r)
            for (int c = 0; c < NUM_COLS; ++c) outProb[r][c] /= localMax;
        return;
    }

    for (int r = 0; r < NUM_ROWS; ++r)
        for (int c = 0; c < NUM_COLS; ++c)
            outProb[r][c] = static_cast<double>(counts[r][c]) / static_cast<double>(maxCount);
}


// Monte-Carlo sampler: randomly place remaining ships consistent with boardView.
// iterations controls sample count. This is slower but often produces robust maps.
void monteCarloProbabilities(const char boardView[NUM_ROWS][NUM_COLS],
                             const int remaining[NUM_SHIPS],
                             int iterations,
                             double outProb[NUM_ROWS][NUM_COLS]) {
    // accumulate counts
    int counts[NUM_ROWS][NUM_COLS] = {0};
    srand(static_cast<unsigned int>(time(nullptr)));

    // Precompute list of ship sizes to place
    vector<int> ships;
    for (int i = 0; i < NUM_SHIPS; ++i) if (remaining[i] > 0) ships.push_back(remaining[i]);
    if (ships.empty()) {
        for (int r = 0; r < NUM_ROWS; ++r)
            for (int c = 0; c < NUM_COLS; ++c) outProb[r][c] = 0.0;
        return;
    }

    for (int it = 0; it < iterations; ++it) {
        // try to place all ships randomly; if fail, skip sample
        char sample[NUM_ROWS][NUM_COLS];
        for (int r = 0; r < NUM_ROWS; ++r)
            for (int c = 0; c < NUM_COLS; ++c) sample[r][c] = boardView[r][c];

        bool ok = true;
        for (int s = 0; s < (int)ships.size(); ++s) {
            int len = ships[s];
            bool placed = false;
            for (int attempt = 0; attempt < 200 && !placed; ++attempt) {
                bool horiz = rand() % 2;
                int r = rand() % NUM_ROWS;
                int c = rand() % NUM_COLS;
                if (!shipFitsAt(sample, r, c, len, horiz)) continue;
                // ensure doesn't overwrite known misses or hits in inconsistent way
                bool conflict = false;
                for (int k = 0; k < len; ++k) {
                    int nr = r + (horiz ? 0 : k);
                    int nc = c + (horiz ? k : 0);
                    if (boardView[nr][nc] == 'm') { conflict = true; break; }
                }
                if (conflict) continue;
                // place with symbol 'S'
                for (int k = 0; k < len; ++k) {
                    int nr = r + (horiz ? 0 : k);
                    int nc = c + (horiz ? k : 0);
                    sample[nr][nc] = 'S';
                }
                placed = true;
            }
            if (!placed) { ok = false; break; }
        }
        if (!ok) continue;

        // accumulate counts for unknown cells
        for (int r = 0; r < NUM_ROWS; ++r)
            for (int c = 0; c < NUM_COLS; ++c)
                if (sample[r][c] == 'S') counts[r][c]++;
    }

    int maxCount = 0;
    for (int r = 0; r < NUM_ROWS; ++r)
        for (int c = 0; c < NUM_COLS; ++c)
            if (counts[r][c] > maxCount) maxCount = counts[r][c];

    if (maxCount == 0) {
        for (int r = 0; r < NUM_ROWS; ++r)
            for (int c = 0; c < NUM_COLS; ++c) outProb[r][c] = 0.0;
        return;
    }
    for (int r = 0; r < NUM_ROWS; ++r)
        for (int c = 0; c < NUM_COLS; ++c)
            outProb[r][c] = static_cast<double>(counts[r][c]) / static_cast<double>(maxCount);
}

/**
 * Pick a cell from the weighted board randomly.
 * The weights are flattened and accumulated, then a random
 * number is generated between 0 and the total weight.
 * The cell corresponding to the first weight that exceeds
 * the random number is returned.
 *
 * @param weights A 2D array of weights, where weights[r][c]
 *             represents the weight of the cell at row r, column c.
 * @return A pair of integers representing the row and column
 *             of the chosen cell.
 */
pair<int,int> pickWeightedCell(double weights[NUM_ROWS][NUM_COLS]) {
    vector<double> flat;
    vector<pair<int,int>> coords;
    flat.reserve(NUM_ROWS * NUM_COLS);
    coords.reserve(NUM_ROWS * NUM_COLS);

    for (int r = 0; r < NUM_ROWS; ++r) {
        for (int c = 0; c < NUM_COLS; ++c) {
            flat.push_back(weights[r][c]);
            coords.push_back({r, c});
        }
    }

    double total = accumulate(flat.begin(), flat.end(), 0.0);
    double rnd = ((double) rand() / RAND_MAX) * total;

    double running = 0.0;
    for (size_t i = 0; i < flat.size(); ++i) {
        running += flat[i];
        if (rnd <= running) return coords[i];
    }
    return coords.back(); // fallback
}
