#ifndef BATTLESHIP_H
#define BATTLESHIP_H

#include <iostream>
#include <string>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <cctype>
#include <vector>
#include <sstream>
#include <algorithm>

using namespace std;

// Global constants
const int NUM_ROWS = 10;
const int NUM_COLS = 10;
const int NUM_SHIPS = 5;
const int cTimeout = 0;
const int pTimeout = 0;

const string SHIP_NAMES[]   = {"carrier", "battleship", "cruiser", "submarine", "destroyer"};
const char   SHIP_SYMBOLS[] = {'c', 'b', 'r', 's', 'd'};
const int    SHIP_SIZES[]   = {5, 4, 3, 3, 2};

// Hit and miss markers
const char HIT = 'X';
const char MISS = 'm';

enum PlayerType { HUMAN, COMPUTER };

// Stats struct
struct Stats {
    int hits = 0;
    int misses = 0;
    int totalShots = 0;
    double hitMissRatio = 0.0;
    bool won = false;
};

struct TargetState {
    bool active = false;
    int lastHitRow = -1;
    int lastHitCol = -1;
    bool oriented = false;
    int orientation = 0; // 0 none, 1 horizontal, 2 vertical
    std::vector<std::pair<int,int>> queue;
};

// Utility
// These are removed for WASM - no console interaction
// void pauseMs(int ms);
// void clearScreen();

// Core functions
void welcomeScreen();
void initializeBoard(char board[NUM_ROWS][NUM_COLS]);
void displayBoard(const char board[NUM_ROWS][NUM_COLS], bool showShips);
void biasedPlaceShipsOnBoard(char board[NUM_ROWS][NUM_COLS]);
void manuallyPlaceShipsOnBoard(char board[NUM_ROWS][NUM_COLS]);
void randomlyPlaceShipsOnBoard(char board[NUM_ROWS][NUM_COLS]);
int  selectWhoStartsFirst();
bool checkShotIsAvailable(const char board[NUM_ROWS][NUM_COLS], int row, int col);
int  updateBoard(char board[NUM_ROWS][NUM_COLS], int row, int col, int shipSizes[]);
bool isWinner(const int shipSizes[]);
// outputCurrentMove removed for WASM - no file I/O
bool updateShipSize(int shipSizes[], int shipIndex);
// outputStats removed for WASM - no file I/O
pair<int,int> getMove(PlayerType type, const char board[NUM_ROWS][NUM_COLS]);
pair<int,int> parseFlexibleInput();

// Helpers
bool canPlaceShip(const char board[NUM_ROWS][NUM_COLS], int row, int col, int size, bool horizontal);
void placeShip(char board[NUM_ROWS][NUM_COLS], int row, int col, int size, char symbol, bool horizontal);
bool isShipSymbol(char ch);

// Conversion helpers
int  letterToCol(char colChar);
char colToLetter(int col);

#endif
