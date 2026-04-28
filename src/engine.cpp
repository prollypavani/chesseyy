#include "engine.h"
#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <queue>
#include <string>
#include <iostream>
using namespace std;

// --- CP Component: Graphs & Topological Sort (Static Exchange Evaluation - SEE) ---
void printSEETopoSort(const Board& board, int targetRow, int targetCol) {
    vector<pair<int, string>> attackers;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (board[r][c] == '.') continue;
            Board temp = board;
            if (isPseudoLegalMove(temp, r, c, targetRow, targetCol)) {
                int val = getPieceValue(temp[r][c]);
                string name = string(1, temp[r][c]) + " at (" + to_string(r) + "," + to_string(c) + ")";
                attackers.push_back({val, name});
            }
        }
    }
    
    if (attackers.size() <= 1) return;
    
    // Build Directed Graph for Topo Sort (lower value -> higher value priority)
    int inDegree[32] = {0};
    vector<int> adj[32];
    for (size_t i = 0; i < attackers.size(); ++i) {
        for (size_t j = 0; j < attackers.size(); ++j) {
            if (i == j) continue;
            if (attackers[i].first < attackers[j].first) {
                adj[i].push_back(j);
                inDegree[j]++;
            } else if (attackers[i].first == attackers[j].first && i < j) {
                // Break ties to ensure strictly acyclic properties
                adj[i].push_back(j);
                inDegree[j]++;
            }
        }
    }
    
    queue<int> q;
    for (size_t i = 0; i < attackers.size(); ++i) {
        if (inDegree[i] == 0) q.push(i);
    }
    
    cout << "\n--- Topological Sort (SEE Captures on " << targetRow << "," << targetCol << ") ---\n";
    cout << "Capture Priority: ";
    size_t count = 0;
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        cout << attackers[u].second;
        count++;
        if (count < attackers.size()) cout << " -> ";
        for (int v : adj[u]) {
            inDegree[v]--;
            if (inDegree[v] == 0) q.push(v);
        }
    }
    cout << "\n";
}

int nodesExplored = 0;
bool useAlphaBeta = true;

// --- CP Component: Number Theory & Bit Manipulation (Zobrist Hashing) ---
uint64_t zobristTable[8][8][12];

enum TTFlag { EXACT, LOWERBOUND, UPPERBOUND };
struct TTEntry {
    int depth;
    int score;
    TTFlag flag;
};

std::unordered_map<uint64_t, TTEntry> transpositionTable;
int ttHits = 0;

int getPieceIndex(char p) {
    switch (p) {
        case 'P': return 0; case 'N': return 1; case 'B': return 2; case 'R': return 3; case 'Q': return 4; case 'K': return 5;
        case 'p': return 6; case 'n': return 7; case 'b': return 8; case 'r': return 9; case 'q': return 10; case 'k': return 11;
        default: return -1;
    }
}

void initZobrist() {
    std::mt19937_64 rng(12345); // Fixed seed
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            for (int p = 0; p < 12; ++p) {
                zobristTable[r][c][p] = rng();
            }
        }
    }
}

void clearTT() {
    transpositionTable.clear();
    ttHits = 0;
}

uint64_t computeZobristHash(const Board& board) {
    uint64_t hash = 0;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (board[r][c] != '.') {
                int pIdx = getPieceIndex(board[r][c]);
                if (pIdx != -1) hash ^= zobristTable[r][c][pIdx];
            }
        }
    }
    return hash;
}

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

int getRangeSum(int prefix[], int L, int R) {
    if (L > R) return 0;
    L = std::max(0, L);
    R = std::min(7, R);
    if (L == 0) return prefix[R];
    return prefix[R] - prefix[L - 1];
}

vector<pair<int,int>> passedPawnsGlobal; // Used for UI visualizations of Prefix Array logic

int evaluateBoard(const Board& board) {
    int score = 0;
    passedPawnsGlobal.clear();
    
    // --- CP Component: Array Techniques (Prefix Sums for Range Queries) ---
    int whitePawnsOnFile[BOARD_SIZE] = {0};
    int blackPawnsOnFile[BOARD_SIZE] = {0};

    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (board[r][c] == 'P') whitePawnsOnFile[c]++;
            if (board[r][c] == 'p') blackPawnsOnFile[c]++;
        }
    }

    int whitePrefix[BOARD_SIZE] = {0};
    int blackPrefix[BOARD_SIZE] = {0};
    whitePrefix[0] = whitePawnsOnFile[0];
    blackPrefix[0] = blackPawnsOnFile[0];
    for (int i = 1; i < BOARD_SIZE; ++i) {
        whitePrefix[i] = whitePrefix[i - 1] + whitePawnsOnFile[i];
        blackPrefix[i] = blackPrefix[i - 1] + blackPawnsOnFile[i];
    }

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
                    if (getRangeSum(blackPrefix, col - 1, col + 1) == 0) {
                        positionalScore += 30; // Passed pawn bonus
                        passedPawnsGlobal.push_back({row, col});
                    }
                } else {
                    positionalScore += (row - 1); // reward black pawns moving down
                    if (getRangeSum(whitePrefix, col - 1, col + 1) == 0) {
                        positionalScore += 30; // Passed pawn bonus
                        passedPawnsGlobal.push_back({row, col});
                    }
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
        // MVV-LVA (Most Valuable Victim - Least Valuable Attacker) heuristic
        score += getPieceValue(targetPiece) * 10 - getPieceValue(movingPiece);
    }
    return score;
}

// --- CP Component: STL and Comparators (Move Ordering) ---
vector<Move> getOrderedMoves(Board& board, const bool isWhiteTurnForMoves) {
    vector<Move> moves = getAllValidMoves(board, isWhiteTurnForMoves);
    
    std::sort(moves.begin(), moves.end(), [&board](const Move& a, const Move& b) {
        return scoreMove(board, a) > scoreMove(board, b); // Custom Comparator
    });
    
    return moves;
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

    uint64_t hash = computeZobristHash(board);
    if (auto it = transpositionTable.find(hash); it != transpositionTable.end()) {
        if (it->second.depth >= depth) {
            ttHits++;
            if (it->second.flag == EXACT) {
                return it->second.score;
            }
            if (it->second.flag == LOWERBOUND) {
                alpha = max(alpha, it->second.score);
            } else if (it->second.flag == UPPERBOUND) {
                beta = min(beta, it->second.score);
            }
            if (alpha >= beta) {
                return it->second.score;
            }
        }
    }

    const vector<Move> moves = getOrderedMoves(board, isMaximizing);
    if (moves.empty()) {
        return evaluateBoard(board);
    }

    int originalAlpha = alpha;
    int originalBeta = beta;

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
        
        TTFlag flag = EXACT;
        if (bestScore <= originalAlpha) flag = UPPERBOUND;
        else if (bestScore >= beta) flag = LOWERBOUND;
        transpositionTable[hash] = {depth, bestScore, flag};
        
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
    
    TTFlag flag = EXACT;
    if (bestScore >= originalBeta) flag = LOWERBOUND;
    else if (bestScore <= alpha) flag = UPPERBOUND;
    transpositionTable[hash] = {depth, bestScore, flag};

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
