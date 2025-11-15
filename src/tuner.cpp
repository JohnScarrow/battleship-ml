#include "Tournament.h"
#include "MLforAI.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <atomic>
#include <sstream>

using namespace std;

// Parse a range spec like "start:step:end" into vector of doubles.
static vector<double> parseRange(const string &spec, double defStart, double defStep, double defEnd) {
    if (spec.empty()) return {defStart};
    size_t a = spec.find(':');
    size_t b = spec.rfind(':');
    if (a == string::npos || b == a) return {stod(spec)};
    double start = stod(spec.substr(0,a));
    double step = stod(spec.substr(a+1,b-a-1));
    double end = stod(spec.substr(b+1));
    vector<double> out;
    for (double v = start; v <= end + 1e-9; v += step) out.push_back(v);
    return out;
}

// Worker: run `games` tournaments and accumulate shot totals
static void runGamesWorker(int games, atomic<long long> &accShotsP1, atomic<long long> &accShotsP2) {
    Tournament t;
    t.start(3, games);
    while (!t.done()) t.tick();
    accShotsP1 += t.shotsP1Accum;
    accShotsP2 += t.shotsP2Accum;
}

int main(int argc, char** argv) {
    srand(static_cast<unsigned int>(time(nullptr)));

    int totalGames = 500; // default per combo
    int threads = 1;
    int online = 0;
    string alphaSpec, placeSpec, adjSpec, mcSpec;

    // Simple CLI: key=value pairs
    for (int i = 1; i < argc; ++i) {
        string s = argv[i];
        size_t eq = s.find('=');
        if (eq == string::npos) continue;
        string k = s.substr(0,eq), v = s.substr(eq+1);
        if (k=="games") totalGames = stoi(v);
        else if (k=="threads") threads = stoi(v);
        else if (k=="alpha") alphaSpec = v;
        else if (k=="place") placeSpec = v;
        else if (k=="adj") adjSpec = v;
        else if (k=="mc") mcSpec = v;
        else if (k=="online") online = stoi(v);
    }

    // Default ranges
    auto alphas = parseRange(alphaSpec, 0.65, 0.05, 0.85);
    auto places = parseRange(placeSpec, 1.0, 0.5, 2.0);
    auto adjs = parseRange(adjSpec, 0.2, 0.2, 0.6);
    auto mcs = parseRange(mcSpec, 0.0, 0.5, 0.5);

    if (online) {
        // Online learning mode: update weights after each game
        cout << "[Online learning mode enabled]" << endl;
        // Start with initial weights (first in range)
        AIWeights w = gAIWeights;
        w.globalAlphaEarly = alphas[0];
        w.placementHitMultiplier = places[0];
        w.adjHitBonus = adjs[0];
        w.mcBlendRatio = mcs[0];
        setAIWeights(w);

        double lr = 0.05; // learning rate
        double bestAvg = 1e9;
        for (int g = 0; g < totalGames; ++g) {
            Tournament t;
            t.start(3, 1);
            while (!t.done()) t.tick();
            double avgShots = 0.5 * (t.shotsP1Accum + t.shotsP2Accum);
            // Simple reward: if avgShots < bestAvg, reinforce weights
            if (avgShots < bestAvg) {
                bestAvg = avgShots;
                // Reward: nudge weights in current direction
                w.globalAlphaEarly += lr * ((rand()%2) ? 1 : -1) * 0.01;
                w.placementHitMultiplier += lr * ((rand()%2) ? 1 : -1) * 0.05;
                w.adjHitBonus += lr * ((rand()%2) ? 1 : -1) * 0.02;
                w.mcBlendRatio += lr * ((rand()%2) ? 1 : -1) * 0.01;
            } else {
                // Penalize: nudge weights in opposite direction
                w.globalAlphaEarly -= lr * ((rand()%2) ? 1 : -1) * 0.01;
                w.placementHitMultiplier -= lr * ((rand()%2) ? 1 : -1) * 0.05;
                w.adjHitBonus -= lr * ((rand()%2) ? 1 : -1) * 0.02;
                w.mcBlendRatio -= lr * ((rand()%2) ? 1 : -1) * 0.01;
            }
            // Clamp weights to reasonable ranges
            w.globalAlphaEarly = max(0.5, min(0.9, w.globalAlphaEarly));
            w.placementHitMultiplier = max(0.5, min(3.0, w.placementHitMultiplier));
            w.adjHitBonus = max(0.0, min(1.0, w.adjHitBonus));
            w.mcBlendRatio = max(0.0, min(1.0, w.mcBlendRatio));
            setAIWeights(w);
            if ((g+1) % 50 == 0) {
                cout << "After " << (g+1) << " games: alpha=" << w.globalAlphaEarly
                     << ", place=" << w.placementHitMultiplier
                     << ", adj=" << w.adjHitBonus
                     << ", mc=" << w.mcBlendRatio
                     << ", bestAvgShots=" << bestAvg << endl;
            }
        }
        cout << "Final weights after online learning:" << endl;
        cout << "alphaEarly,placementHitMultiplier,adjHitBonus,mcBlendRatio" << endl;
        cout << fixed << setprecision(4)
             << w.globalAlphaEarly << ","
             << w.placementHitMultiplier << ","
             << w.adjHitBonus << ","
             << w.mcBlendRatio << endl;
        return 0;
    }

    // Default: sweep mode
    cout << "alphaEarly,placementHitMultiplier,adjHitBonus,mcBlendRatio,games,threads,p1_avg_shots,p2_avg_shots" << endl;

    for (double alpha : alphas) {
        for (double pm : places) {
            for (double ab : adjs) {
                for (double mb : mcs) {
                    AIWeights w = gAIWeights;
                    w.globalAlphaEarly = alpha;
                    w.placementHitMultiplier = pm;
                    w.adjHitBonus = ab;
                    w.mcBlendRatio = mb;
                    setAIWeights(w);

                    // Split totalGames across threads
                    int perThread = max(1, totalGames / threads);
                    atomic<long long> accP1{0}, accP2{0};
                    vector<thread> ths;
                    for (int t = 0; t < threads; ++t) {
                        ths.emplace_back(runGamesWorker, perThread, std::ref(accP1), std::ref(accP2));
                    }
                    for (auto &th : ths) th.join();

                    double p1avg = totalGames ? double(accP1.load())/totalGames : 0.0;
                    double p2avg = totalGames ? double(accP2.load())/totalGames : 0.0;
                    cout << fixed << setprecision(3)
                         << alpha << "," << pm << "," << ab << "," << mb << ","
                         << totalGames << "," << threads << "," << p1avg << "," << p2avg << endl;
                }
            }
        }
    }

    return 0;
}
