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
    }

    // Default ranges
    auto alphas = parseRange(alphaSpec, 0.65, 0.05, 0.85);
    auto places = parseRange(placeSpec, 1.0, 0.5, 2.0);
    auto adjs = parseRange(adjSpec, 0.2, 0.2, 0.6);
    auto mcs = parseRange(mcSpec, 0.0, 0.5, 0.5);

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
