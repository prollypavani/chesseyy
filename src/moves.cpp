#include "moves.h"

#include <cctype>
#include <cmath>
using namespace std;

bool isInsideBoard(const int row, const int col) {
    return row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE;
}

bool isPathClear(const Board& board, int fromRow, int fromCol, const int toRow, const int toCol) {
    int rowStep = 0;
    if (toRow > fromRow) {
        rowStep = 1;
    } else if (toRow < fromRow) {
        rowStep = -1;
    }

    int colStep = 0;
    if (toCol > fromCol) {
        colStep = 1;
    } else if (toCol < fromCol) {
        colStep = -1;
    }

    int row = fromRow + rowStep;
    int col = fromCol + colStep;
    while (row != toRow || col != toCol) {
        if (board[row][col] != '.') {
            return false;
        }
        row += rowStep;
        col += colStep;
    }
    return true;
}

bool isPseudoLegalMove(const Board& board, const int fromRow, const int fromCol, const int toRow, const int toCol) {
    if (!isInsideBoard(fromRow, fromCol) || !isInsideBoard(toRow, toCol)) {
        return false;
    }

    if (fromRow == toRow && fromCol == toCol) {
        return false;
    }

    const char movingPiece = board[fromRow][fromCol];
    if (movingPiece == '.') {
        return false;
    }

    const char targetPiece = board[toRow][toCol];
    if (targetPiece != '.') {
        const bool movingIsWhite = isupper(static_cast<unsigned char>(movingPiece)) != 0;
        const bool targetIsWhite = isupper(static_cast<unsigned char>(targetPiece)) != 0;
        if (movingIsWhite == targetIsWhite) {
            return false;
        }
    }

    const int rowDelta = toRow - fromRow;
    const int colDelta = toCol - fromCol;
    const int absRowDelta = abs(rowDelta);
    const int absColDelta = abs(colDelta);

    switch (tolower(static_cast<unsigned char>(movingPiece))) {
        case 'p': {
            const bool isWhite = isupper(static_cast<unsigned char>(movingPiece)) != 0;
            const int forward = isWhite ? -1 : 1;
            const int startRow = isWhite ? 6 : 1;

            if (colDelta == 0) {
                if (rowDelta == forward && targetPiece == '.') {
                    return true;
                }
                if (fromRow == startRow && rowDelta == 2 * forward && targetPiece == '.') {
                    const int middleRow = fromRow + forward;
                    return board[middleRow][fromCol] == '.';
                }
                return false;
            }

            if (absColDelta == 1 && rowDelta == forward) {
                return targetPiece != '.';
            }
            return false;
        }
        case 'n':
            return (absRowDelta == 2 && absColDelta == 1) || (absRowDelta == 1 && absColDelta == 2);
        case 'r':
            if (fromRow != toRow && fromCol != toCol) {
                return false;
            }
            return isPathClear(board, fromRow, fromCol, toRow, toCol);
        case 'b':
            if (absRowDelta != absColDelta) {
                return false;
            }
            return isPathClear(board, fromRow, fromCol, toRow, toCol);
        case 'q': {
            const bool straight = (fromRow == toRow || fromCol == toCol);
            const bool diagonal = (absRowDelta == absColDelta);
            if (!straight && !diagonal) {
                return false;
            }
            return isPathClear(board, fromRow, fromCol, toRow, toCol);
        }
        case 'k':
            return absRowDelta <= 1 && absColDelta <= 1;
        default:
            return false;
    }
}

bool isKingInCheck(const Board& board, const bool isWhite) {
    int kingRow = -1;
    int kingCol = -1;
    const char kingChar = isWhite ? 'K' : 'k';

    for (int row = 0; row < BOARD_SIZE; ++row) {
        for (int col = 0; col < BOARD_SIZE; ++col) {
            if (board[row][col] == kingChar) {
                kingRow = row;
                kingCol = col;
            }
        }
    }

    if (kingRow == -1 || kingCol == -1) {
        return false;
    }

    for (int row = 0; row < BOARD_SIZE; ++row) {
        for (int col = 0; col < BOARD_SIZE; ++col) {
            const char piece = board[row][col];
            if (piece == '.') {
                continue;
            }

            const bool pieceIsWhite = isupper(static_cast<unsigned char>(piece)) != 0;
            if (pieceIsWhite == isWhite) {
                continue;
            }

            if (isPseudoLegalMove(board, row, col, kingRow, kingCol)) {
                return true;
            }
        }
    }

    return false;
}

bool isValidMove(Board& board, const int fromRow, const int fromCol, const int toRow, const int toCol) {
    if (!isPseudoLegalMove(board, fromRow, fromCol, toRow, toCol)) {
        return false;
    }

    const char movingPiece = board[fromRow][fromCol];
    const char capturedPiece = board[toRow][toCol];
    const bool movingPieceIsWhite = isupper(static_cast<unsigned char>(movingPiece)) != 0;

    board[toRow][toCol] = movingPiece;
    board[fromRow][fromCol] = '.';

    const bool ownKingInCheck = isKingInCheck(board, movingPieceIsWhite);

    board[fromRow][fromCol] = movingPiece;
    board[toRow][toCol] = capturedPiece;

    return !ownKingInCheck;
}

vector<Move> getAllValidMoves(Board& board, const bool isWhiteTurnForMoves) {
    vector<Move> moves;
    for (int fromRow = 0; fromRow < BOARD_SIZE; ++fromRow) {
        for (int fromCol = 0; fromCol < BOARD_SIZE; ++fromCol) {
            const char piece = board[fromRow][fromCol];
            if (piece == '.') {
                continue;
            }

            const bool pieceIsWhite = isupper(static_cast<unsigned char>(piece)) != 0;
            if (pieceIsWhite != isWhiteTurnForMoves) {
                continue;
            }

            for (int toRow = 0; toRow < BOARD_SIZE; ++toRow) {
                for (int toCol = 0; toCol < BOARD_SIZE; ++toCol) {
                    if (!isValidMove(board, fromRow, fromCol, toRow, toCol)) {
                        continue;
                    }
                    moves.push_back(Move{fromRow, fromCol, toRow, toCol});
                }
            }
        }
    }
    return moves;
}

bool hasAnyValidMoves(Board& board, const bool isWhite) {
    return !getAllValidMoves(board, isWhite).empty();
}

char makeMove(Board& board, const Move& move) {
    const char capturedPiece = board[move.toRow][move.toCol];
    board[move.toRow][move.toCol] = board[move.fromRow][move.fromCol];
    board[move.fromRow][move.fromCol] = '.';
    return capturedPiece;
}

void undoMove(Board& board, const Move& move, const char capturedPiece) {
    board[move.fromRow][move.fromCol] = board[move.toRow][move.toCol];
    board[move.toRow][move.toCol] = capturedPiece;
}
