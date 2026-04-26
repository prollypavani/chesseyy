#include "engine.h"

#include <algorithm>
#include <cctype>
#include <climits>
using namespace std;

int nodesExplored = 0;
bool useAlphaBeta = true;

int getPieceValue(const char piece) {
    switch (tolower(static_cast<unsigned char>(piece))) {
        case 'p':
            return 1;
        case 'n':
            return 3;
        case 'b':
            return 3;
        case 'r':
            return 5;
        case 'q':
            return 9;
        case 'k':
            return 100;
        default:
            return 0;
    }
}

int evaluateBoard(const Board& board) {
    int score = 0;
    for (int row = 0; row < BOARD_SIZE; ++row) {
        for (int col = 0; col < BOARD_SIZE; ++col) {
            const char piece = board[row][col];
            if (piece == '.') {
                continue;
            }

            const int materialScore = getPieceValue(piece) * 10;
            const bool isWhite = isupper(static_cast<unsigned char>(piece)) != 0;

            int positionalScore = 0;

            const bool nearCenter = (row >= 2 && row <= 5 && col >= 2 && col <= 5);
            if (nearCenter) {
                positionalScore += 1; // +0.1
            }

            if (tolower(static_cast<unsigned char>(piece)) == 'p') {
                if (isWhite) {
                    positionalScore += (6 - row); // reward white pawns moving up
                } else {
                    positionalScore += (row - 1); // reward black pawns moving down
                }
            }

            const int totalPieceScore = materialScore + positionalScore;
            score += isWhite ? totalPieceScore : -totalPieceScore;
        }
    }
    return score;
}

namespace {
int scoreMove(const Board& board, const Move& move) {
    const char movingPiece = board[move.fromRow][move.fromCol];
    const char targetPiece = board[move.toRow][move.toCol];

    int score = 0;
    if (targetPiece != '.') {
        score += getPieceValue(targetPiece) * 10 - getPieceValue(movingPiece);
    }
    return score;
}

vector<Move> getOrderedMoves(Board& board, const bool isWhiteTurnForMoves) {
    const vector<Move> moves = getAllValidMoves(board, isWhiteTurnForMoves);
    vector<pair<int, Move>> scoredMoves;
    scoredMoves.reserve(moves.size());

    for (const Move& move : moves) {
        const int moveScore = scoreMove(board, move);
        const auto insertPosition = lower_bound(
            scoredMoves.begin(),
            scoredMoves.end(),
            moveScore,
            [](const pair<int, Move>& entry, const int score) {
                return entry.first > score;
            }
        );
        scoredMoves.insert(insertPosition, {moveScore, move});
    }

    vector<Move> orderedMoves;
    orderedMoves.reserve(scoredMoves.size());
    for (const auto& [score, move] : scoredMoves) {
        (void)score;
        orderedMoves.push_back(move);
    }
    return orderedMoves;
}
}

int minimax_plain(Board& board, const int depth, const bool isMaximizing) {
    ++nodesExplored;

    if (depth == 0) {
        return evaluateBoard(board);
    }

    const vector<Move> moves = getOrderedMoves(board, isMaximizing);
    if (moves.empty()) {
        return evaluateBoard(board);
    }

    if (isMaximizing) {
        int bestScore = INT_MIN;
        for (const Move& move : moves) {
            const char capturedPiece = makeMove(board, move);
            const int score = minimax_plain(board, depth - 1, false);
            undoMove(board, move, capturedPiece);
            if (score > bestScore) {
                bestScore = score;
            }
        }
        return bestScore;
    }

    int bestScore = INT_MAX;
    for (const Move& move : moves) {
        const char capturedPiece = makeMove(board, move);
        const int score = minimax_plain(board, depth - 1, true);
        undoMove(board, move, capturedPiece);
        if (score < bestScore) {
            bestScore = score;
        }
    }
    return bestScore;
}

int minimax_ab(Board& board, const int depth, int alpha, int beta, const bool isMaximizing) {
    ++nodesExplored;

    if (depth == 0) {
        return evaluateBoard(board);
    }

    const vector<Move> moves = getOrderedMoves(board, isMaximizing);
    if (moves.empty()) {
        return evaluateBoard(board);
    }

    if (isMaximizing) {
        int bestScore = INT_MIN;
        for (const Move& move : moves) {
            const char capturedPiece = makeMove(board, move);
            const int score = minimax_ab(board, depth - 1, alpha, beta, false);
            undoMove(board, move, capturedPiece);
            if (score > bestScore) {
                bestScore = score;
            }
            if (bestScore > alpha) {
                alpha = bestScore;
            }
            if (beta <= alpha) {
                break;
            }
        }
        return bestScore;
    }

    int bestScore = INT_MAX;
    for (const Move& move : moves) {
        const char capturedPiece = makeMove(board, move);
        const int score = minimax_ab(board, depth - 1, alpha, beta, true);
        undoMove(board, move, capturedPiece);
        if (score < bestScore) {
            bestScore = score;
        }
        if (bestScore < beta) {
            beta = bestScore;
        }
        if (beta <= alpha) {
            break;
        }
    }
    return bestScore;
}

Move getBestMove(Board& board, const int depth) {
    const vector<Move> blackMoves = getOrderedMoves(board, false);
    Move bestMove{0, 0, 0, 0};
    int bestScore = INT_MAX;

    for (const Move& move : blackMoves) {
        const char capturedPiece = makeMove(board, move);
        int score = 0;
        if (useAlphaBeta) {
            score = minimax_ab(board, depth - 1, INT_MIN, INT_MAX, true);
        } else {
            score = minimax_plain(board, depth - 1, true);
        }
        undoMove(board, move, capturedPiece);

        if (score < bestScore) {
            bestScore = score;
            bestMove = move;
        }
    }

    return bestMove;
}
