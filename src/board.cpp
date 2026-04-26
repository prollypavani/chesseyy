#include "board.h"

#include <iostream>
using namespace std;

Board createInitialBoard() {
    return {{
        {{'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'}},
        {{'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'}},
        {{'.', '.', '.', '.', '.', '.', '.', '.'}},
        {{'.', '.', '.', '.', '.', '.', '.', '.'}},
        {{'.', '.', '.', '.', '.', '.', '.', '.'}},
        {{'.', '.', '.', '.', '.', '.', '.', '.'}},
        {{'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'}},
        {{'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'}}
    }};
}

void printBoard(const Board& board) {
    cout << "Chess board:\n";
    for (const auto& row : board) {
        for (const char piece : row) {
            cout << piece << ' ';
        }
        cout << '\n';
    }
}
