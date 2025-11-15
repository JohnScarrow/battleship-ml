
#include "battleship.h"
#include "MLforAI.h"
#include <limits>
#include <iomanip>


//for multithreading
#include <thread>
#include <vector>
#include <atomic>


using namespace std;




struct RoundResult {
    int p1Wins = 0;
    int p2Wins = 0;
    long long shotsP1 = 0;
    long long shotsP2 = 0;
};

RoundResult playOneRound(int mode, int round) {
    RoundResult result;

    ofstream logFile("battleship.log", ios::app); // append each round

    PlayerType player1Type, player2Type;
    if (mode == 1) { player1Type = HUMAN; player2Type = HUMAN; }
    else if (mode == 2) { player1Type = HUMAN; player2Type = COMPUTER; }
    else { player1Type = COMPUTER; player2Type = COMPUTER; }

    // Boards
    char playerBoard[NUM_ROWS][NUM_COLS];
    char computerBoard[NUM_ROWS][NUM_COLS];
    initializeBoard(playerBoard);
    initializeBoard(computerBoard);

    // Placement
    biasedPlaceShipsOnBoard(playerBoard);
    biasedPlaceShipsOnBoard(computerBoard);

    // Ship health
    int playerShipSizes[NUM_SHIPS];
    int computerShipSizes[NUM_SHIPS];
    for (int i = 0; i < NUM_SHIPS; ++i) {
        playerShipSizes[i] = SHIP_SIZES[i];
        computerShipSizes[i] = SHIP_SIZES[i];
    }

    // Stats
    Stats playerStats, computerStats;

    // Learning arrays
    int hitCount[NUM_ROWS][NUM_COLS] = {0};
    int missCount[NUM_ROWS][NUM_COLS] = {0};
    double hitProb[NUM_ROWS][NUM_COLS] = {0};
    int liveHits[NUM_ROWS][NUM_COLS] = {0};
    int liveMisses[NUM_ROWS][NUM_COLS] = {0};
    double liveProb[NUM_ROWS][NUM_COLS] = {0};

    learnFromLog("battleship.log", hitCount, missCount);
    computeProbabilities(hitCount, missCount, hitProb);
    saveHeatmap("heatmap.txt", hitProb);

    // Target states
    TargetState p1Target, p2Target;

    int turn = selectWhoStartsFirst();
    bool gameOver = false;
    int turnCount = 0;


    while (!gameOver) {
        PlayerType currentType = (turn == 0 ? player1Type : player2Type);
        char (*targetBoard)[NUM_COLS] = (turn == 0 ? computerBoard : playerBoard);
        int *targetShipSizes = (turn == 0 ? computerShipSizes : playerShipSizes);
        Stats &currentStats = (turn == 0 ? playerStats : computerStats);
        string currentName = (turn == 0 ? "Player1" : "Player2");

        int row = -1, col = -1;
        if (currentType == HUMAN) {
            tie(row, col) = getMove(HUMAN, targetBoard);
        } else {
            TargetState &ts = (turn == 0 ? p1Target : p2Target);
            tie(row, col) = chooseAIMove(targetBoard, hitProb, liveProb, ts, targetShipSizes, turnCount);
            if (!checkShotIsAvailable(targetBoard, row, col)) {
                ts.active = false; ts.oriented = false; ts.orientation = 0; ts.queue.clear();
                tie(row, col) = getSmartMove(targetBoard, hitProb);
            }
        }

        if (!checkShotIsAvailable(targetBoard, row, col)) {
            if (currentType == HUMAN) continue;
            else { turn = 1 - turn; continue; }
        }

        int result = updateBoard(targetBoard, row, col, targetShipSizes);
        bool sunk = false;
        if (result != -1) {
            sunk = updateShipSize(targetShipSizes, result);
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
        outputCurrentMove(logFile, currentName, row, col, result, sunk);

        if (currentType == COMPUTER) {
            TargetState &ts = (turn == 0 ? p1Target : p2Target);
            updateTargetStateAfterResult(ts, targetBoard, row, col, result, sunk, targetShipSizes);
        }

        if (isWinner(targetShipSizes)) {
            gameOver = true;
            currentStats.won = true;
            (turn == 0 ? computerStats : playerStats).won = false;
            cout << currentName << " Wins Round " << round << "!\n";
            logFile << "\n" << currentName << " wins round " << round << "\n";
            break;
        }
        turnCount++;
        turn = 1 - turn;
    }

    outputStats(logFile, playerStats, "Player1");
    outputStats(logFile, computerStats, "Player2");
    logFile.close();

    // Fill in result using Option 2 (direct cast from bool to int)
    result.p1Wins = static_cast<int>(playerStats.won);
    result.p2Wins = static_cast<int>(computerStats.won);
    result.shotsP1 = playerStats.totalShots;
    result.shotsP2 = computerStats.totalShots;

    return result;
}




int main() {
    srand(static_cast<unsigned int>(time(nullptr)));
    std::atomic<int> roundsCompleted(0);


    cout << "Select game mode:\n";
    cout << "1. Player vs Player\n";
    cout << "2. Player vs Computer\n";
    cout << "3. Computer vs Computer\n";
    int mode; 
    cin >> mode;

    cout << "How many rounds do you want to play? ";
    int totalRounds; 
    cin >> totalRounds;

    std::atomic<int> p1Wins(0), p2Wins(0);
    std::atomic<long long> totalShotsP1(0), totalShotsP2(0);


    if (mode == 3) {


    int maxThreads = std::thread::hardware_concurrency();
    // maxThreads = maxThreads - (maxThreads / 4); // use half of available cores

    // maxThreads = maxThreads / 2;  // use half of available cores
    if (maxThreads == 0) maxThreads = 4; // fallback

    // Monitor thread for live stats
    thread monitor([&]() {
        while (roundsCompleted < totalRounds) {
            cout << "\033[2J\033[1;1H";
            cout << "=== Tournament Progress ===\n";
            cout << "Rounds Completed: " << roundsCompleted << " / " << totalRounds << "\n";
            cout << "Player1 Wins: " << p1Wins << "\n";
            cout << "Player2 Wins: " << p2Wins << "\n";
            cout << fixed << setprecision(2);
            cout << "Player1 Avg Shots: " 
                 << (roundsCompleted > 0 ? (double)totalShotsP1 / roundsCompleted : 0.0) << "\n";
            cout << "Player2 Avg Shots: " 
                 << (roundsCompleted > 0 ? (double)totalShotsP2 / roundsCompleted : 0.0) << "\n";
            this_thread::sleep_for(chrono::milliseconds(100)); // Default 100ms
        }
    });

    // Batched thread execution
    int round = 1;
    while (round <= totalRounds) {
        vector<thread> batch;
        int batchSize = min(maxThreads, totalRounds - round + 1);

        for (int i = 0; i < batchSize; ++i, ++round) {
            batch.emplace_back([mode, round, &p1Wins, &p2Wins, &totalShotsP1, &totalShotsP2, &roundsCompleted]() {
                RoundResult r = playOneRound(mode, round);
                p1Wins += r.p1Wins;
                p2Wins += r.p2Wins;
                totalShotsP1 += r.shotsP1;
                totalShotsP2 += r.shotsP2;
                roundsCompleted++;
            });
        }

        for (auto& t : batch) t.join();
    }

    monitor.join(); // Wait for monitor to finish

    // Final summary
    cout << "\033[2J\033[1;1H";
    cout << "\n=== Tournament Results ===\n";
    cout << "Player1 Wins: " << p1Wins << "\n";
    cout << "Player2 Wins: " << p2Wins << "\n";
    cout << "Player1 Avg Shots: " << (double)totalShotsP1 / totalRounds << "\n";
    cout << "Player2 Avg Shots: " << (double)totalShotsP2 / totalRounds << "\n";
} else {
    for (int round = 1; round <= totalRounds; ++round) {
        RoundResult r = playOneRound(mode, round);
        p1Wins += r.p1Wins;
        p2Wins += r.p2Wins;
        totalShotsP1 += r.shotsP1;
        totalShotsP2 += r.shotsP2;

        // Optional: live stats
        cout << "\033[2J\033[1;1H";
        cout << "=== Tournament Progress ===\n";
        cout << "Rounds Completed: " << round << " / " << totalRounds << "\n";
        cout << "Player1 Wins: " << p1Wins << "\n";
        cout << "Player2 Wins: " << p2Wins << "\n";
        cout << fixed << setprecision(2);
        cout << "Player1 Avg Shots: " << (double)totalShotsP1 / round << "\n";
        cout << "Player2 Avg Shots: " << (double)totalShotsP2 / round << "\n";
        this_thread::sleep_for(chrono::milliseconds(50)); // Default 100ms
    }
}



    // Display results
    cout << "\n=== Tournament Results ===\n";
    cout << "Player1 Wins: " << p1Wins << "\n";
    cout << "Player2 Wins: " << p2Wins << "\n";
    cout << "Player1 Avg Shots: " 
         << (totalRounds > 0 ? (double)totalShotsP1 / totalRounds : 0.0) << "\n";
    cout << "Player2 Avg Shots: " 
         << (totalRounds > 0 ? (double)totalShotsP2 / totalRounds : 0.0) << "\n";

    return 0;
}

