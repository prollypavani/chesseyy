#pragma once

#include <array>
using namespace std;

constexpr int BOARD_SIZE = 8;
using Board = array<array<char, BOARD_SIZE>, BOARD_SIZE>;

Board createInitialBoard();
void printBoard(const Board& board);
