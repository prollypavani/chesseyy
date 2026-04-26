#pragma once

#include "board.h"

#include <vector>
using namespace std;

struct Move {
    int fromRow;
    int fromCol;
    int toRow;
    int toCol;
};

bool isInsideBoard(int row, int col);
bool isPathClear(const Board& board, int fromRow, int fromCol, int toRow, int toCol);
bool isPseudoLegalMove(const Board& board, int fromRow, int fromCol, int toRow, int toCol);
bool isKingInCheck(const Board& board, bool isWhite);
bool isValidMove(Board& board, int fromRow, int fromCol, int toRow, int toCol);
vector<Move> getAllValidMoves(Board& board, bool isWhiteTurnForMoves);
bool hasAnyValidMoves(Board& board, bool isWhite);
char makeMove(Board& board, const Move& move);
void undoMove(Board& board, const Move& move, char capturedPiece);
