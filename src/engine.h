#pragma once

#include "board.h"
#include "moves.h"
using namespace std;

extern int nodesExplored;
extern bool useAlphaBeta;
extern int ttHits;
extern std::vector<std::pair<int,int>> passedPawnsGlobal;

void initZobrist();
void printSEETopoSort(const Board& board, int targetRow, int targetCol);
void clearTT();
int getPieceValue(char piece);
int evaluateBoard(const Board& board);
int minimax_plain(Board& board, int depth, bool isMaximizing);
int minimax_ab(Board& board, int depth, int alpha, int beta, bool isMaximizing);
Move getBestMove(Board& board, int depth);
