#include "MLforAI.h"
#include "battleship.h"

using namespace std;

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
void enqueueNeighbors(TargetState &ts, const char board[NUM_ROWS][NUM_COLS], int r, int c) {
    ts.queue.clear();
    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};
    for (int k = 0; k < 4; ++k) {
        int nr = r + dr[k], nc = c + dc[k];
        if (isCellAvailable(board, nr, nc)) {
            ts.queue.emplace_back(nr, nc);
        }
    }
}

// Extend in a line along the current orientation, both directions
void enqueueOrientedLine(TargetState &ts,
                         const char targetBoard[NUM_ROWS][NUM_COLS]) {
    int r = ts.lastHitRow;
    int c = ts.lastHitCol;

    ts.queue.clear();

    if (ts.orientation == 1) { // horizontal
        // left
        for (int cc = c - 1; cc >= 0; --cc) {
            if (checkShotIsAvailable(targetBoard, r, cc)) {
                ts.queue.push_back({r, cc});
            } else if (targetBoard[r][cc] != 'X') break;
        }
        // right
        for (int cc = c + 1; cc < NUM_COLS; ++cc) {
            if (checkShotIsAvailable(targetBoard, r, cc)) {
                ts.queue.push_back({r, cc});
            } else if (targetBoard[r][cc] != 'X') break;
        }
    } else if (ts.orientation == 2) { // vertical
        // up
        for (int rr = r - 1; rr >= 0; --rr) {
            if (checkShotIsAvailable(targetBoard, rr, c)) {
                ts.queue.push_back({rr, c});
            } else if (targetBoard[rr][c] != 'X') break;
        }
        // down
        for (int rr = r + 1; rr < NUM_ROWS; ++rr) {
            if (checkShotIsAvailable(targetBoard, rr, c)) {
                ts.queue.push_back({rr, c});
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
            if (globalProb[r][c] > bestScore) {
                bestScore = globalProb[r][c];
                best = mv;
            }
        }
        ts.queue.clear();
        if (best.first != -1) return best;
        // If queue was invalid, reset targeting
        ts.active = false; ts.oriented = false; ts.orientation = 0;
    }

    // --- Search mode: hybrid scoring ---
    double bestScore = -1.0;
    pair<int,int> bestMove = {-1,-1};

    for (int r = 0; r < NUM_ROWS; ++r) {
        for (int c = 0; c < NUM_COLS; ++c) {
            double s = scoreCell(r, c, board, globalProb, liveProb, remaining, turn);
            if (s > bestScore) {
                bestScore = s;
                bestMove = {r, c};
            }
        }
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
    if (!canFit) score -= 0.5; // Penalize cells that can't fit smallest ship

    
    // Dynamic heatmap weighting with smoother decay and stronger tactical bias
    double alpha = (turn < 10) ? 0.75 : 0.55; // Global
    double beta = 1.0 - alpha; // Live
    double decay = exp(-0.02 * turn);  // smoother fade over time

    score += alpha * globalProb[r][c];
    score += beta * liveProb[r][c] * decay;

    // Tactical streak bonus
    if (liveProb[r][c] > 0.0) {
        score += 0.05;  // small bonus for tactical relevance
    }


    bool bigShipLeft = false;
    for (int i = 0; i < NUM_SHIPS; ++i) if (remaining[i] >= 3) { bigShipLeft = true; break; }
    if (bigShipLeft) {
        if ((r + c) % 2 == 0) score += 0.25;
        else score -= 0.15;    }


// Cardinal Adjacency Loop
    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};
    int adjHits = 0;
    for (int k = 0; k < 4; ++k) {
        int nr = r + dr[k], nc = c + dc[k];
        if (nr >= 0 && nr < NUM_ROWS && nc >= 0 && nc < NUM_COLS) {
            if (board[nr][nc] == 'X') {
                adjHits++;
                score += 0.4;

                // Bonus for extending in same direction
                int nnr = nr + dr[k], nnc = nc + dc[k];
                if (nnr >= 0 && nnr < NUM_ROWS && nnc >= 0 && nnc < NUM_COLS) {
                    if (board[nnr][nnc] == 'X') score += 0.3;
                }
            }
        }
    }

    const int dr_diag[4] = {-1, -1, 1, 1};
const int dc_diag[4] = {-1, 1, -1, 1};
for (int k = 0; k < 4; ++k) {
    int nr = r + dr_diag[k], nc = c + dc_diag[k];
    if (nr >= 0 && nr < NUM_ROWS && nc >= 0 && nc < NUM_COLS) {
        if (board[nr][nc] == 'X') score += 0.2;
    }
}



    if (adjHits > 2) {
        double fitScore = shipFitBiasScoreAt(board, r, c, remaining);
        score += 0.30 * fitScore;  // Only boost fit if near a hit
    }

    score += adjHits * 0.6;
    score += 0.20 * fitScore; //Change this to tune influence of fit score Default is 0.05
    
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
