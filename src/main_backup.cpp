#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <array>
#include <cctype>
#include <climits>
#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

struct Move {
    int fromRow;
    int fromCol;
    int toRow;
    int toCol;
};

int main() {
    constexpr int windowSize = 640;
    constexpr int boardSize = 8;
    constexpr int squareSize = windowSize / boardSize; // 80

    std::array<std::array<char, boardSize>, boardSize> board = {{
        {{'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'}},
        {{'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'}},
        {{'.', '.', '.', '.', '.', '.', '.', '.'}},
        {{'.', '.', '.', '.', '.', '.', '.', '.'}},
        {{'.', '.', '.', '.', '.', '.', '.', '.'}},
        {{'.', '.', '.', '.', '.', '.', '.', '.'}},
        {{'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'}},
        {{'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'}}
    }};

    std::cout << "Chess board:\n";
    for (const auto& row : board) {
        for (const char piece : row) {
            std::cout << piece << ' ';
        }
        std::cout << '\n';
    }

    const sf::Color lightSquare(240, 217, 181);
    const sf::Color darkSquare(181, 136, 99);

    const std::unordered_map<char, std::string> pieceFiles = {
        {'P', "assets/wp.png"}, {'R', "assets/wr.png"}, {'N', "assets/wn.png"},
        {'B', "assets/wb.png"}, {'Q', "assets/wq.png"}, {'K', "assets/wk.png"},
        {'p', "assets/bp.png"}, {'r', "assets/br.png"}, {'n', "assets/bn.png"},
        {'b', "assets/bb.png"}, {'q', "assets/bq.png"}, {'k', "assets/bk.png"}
    };

    std::unordered_map<char, sf::Texture> pieceTextures;
    for (const auto& [piece, path] : pieceFiles) {
        sf::Texture texture;
        if (!texture.loadFromFile(path)) {
            std::cerr << "Failed to load texture: " << path << '\n';
            return 1;
        }
        pieceTextures.emplace(piece, std::move(texture));
    }

    sf::SoundBuffer moveBuffer;
    if (!moveBuffer.loadFromFile("assets/move.wav")) {
        std::cerr << "Failed to load sound: assets/move.wav\n";
        return 1;
    }
    sf::Sound moveSound(moveBuffer);

    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(windowSize, windowSize)),
        "SFML Chessboard"
    );
    window.setFramerateLimit(60);

    bool whiteTurn = true;
    bool hasSelection = false;
    int selectedRow = -1;
    int selectedCol = -1;
    bool hasLastMove = false;
    Move lastMove{0, 0, 0, 0};
    bool gameOver = false;

    const auto isInsideBoard = [&](const int row, const int col) {
        return row >= 0 && row < boardSize && col >= 0 && col < boardSize;
    };

    const auto isPathClear = [&](int fromRow, int fromCol, int toRow, int toCol) {
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
    };

    const auto isPseudoLegalMove = [&](int fromRow, int fromCol, int toRow, int toCol) -> bool {
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
            const bool movingIsWhite = std::isupper(static_cast<unsigned char>(movingPiece)) != 0;
            const bool targetIsWhite = std::isupper(static_cast<unsigned char>(targetPiece)) != 0;
            if (movingIsWhite == targetIsWhite) {
                return false;
            }
        }

        const int rowDelta = toRow - fromRow;
        const int colDelta = toCol - fromCol;
        const int absRowDelta = std::abs(rowDelta);
        const int absColDelta = std::abs(colDelta);

        switch (std::tolower(static_cast<unsigned char>(movingPiece))) {
            case 'p': {
                const bool isWhite = std::isupper(static_cast<unsigned char>(movingPiece)) != 0;
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
                return isPathClear(fromRow, fromCol, toRow, toCol);
            case 'b':
                if (absRowDelta != absColDelta) {
                    return false;
                }
                return isPathClear(fromRow, fromCol, toRow, toCol);
            case 'q': {
                const bool straight = (fromRow == toRow || fromCol == toCol);
                const bool diagonal = (absRowDelta == absColDelta);
                if (!straight && !diagonal) {
                    return false;
                }
                return isPathClear(fromRow, fromCol, toRow, toCol);
            }
            case 'k':
                return absRowDelta <= 1 && absColDelta <= 1;
            default:
                return false;
        }
    };

    const auto isKingInCheck = [&](bool isWhite) {
        int kingRow = -1;
        int kingCol = -1;
        const char kingChar = isWhite ? 'K' : 'k';

        for (int row = 0; row < boardSize; ++row) {
            for (int col = 0; col < boardSize; ++col) {
                if (board[row][col] == kingChar) {
                    kingRow = row;
                    kingCol = col;
                }
            }
        }

        if (kingRow == -1 || kingCol == -1) {
            return false;
        }

        for (int row = 0; row < boardSize; ++row) {
            for (int col = 0; col < boardSize; ++col) {
                const char piece = board[row][col];
                if (piece == '.') {
                    continue;
                }

                const bool pieceIsWhite = std::isupper(static_cast<unsigned char>(piece)) != 0;
                if (pieceIsWhite == isWhite) {
                    continue;
                }

                if (isPseudoLegalMove(row, col, kingRow, kingCol)) {
                    return true;
                }
            }
        }

        return false;
    };

    const auto isValidMove = [&](int fromRow, int fromCol, int toRow, int toCol) -> bool {
        if (!isPseudoLegalMove(fromRow, fromCol, toRow, toCol)) {
            return false;
        }

        const char movingPiece = board[fromRow][fromCol];
        const char capturedPiece = board[toRow][toCol];
        const bool movingPieceIsWhite = std::isupper(static_cast<unsigned char>(movingPiece)) != 0;

        board[toRow][toCol] = movingPiece;
        board[fromRow][fromCol] = '.';

        const bool ownKingInCheck = isKingInCheck(movingPieceIsWhite);

        board[fromRow][fromCol] = movingPiece;
        board[toRow][toCol] = capturedPiece;

        return !ownKingInCheck;
    };

    const auto getPieceValue = [](char piece) {
        switch (std::tolower(static_cast<unsigned char>(piece))) {
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
    };

    const auto evaluateBoard = [&]() {
        int score = 0;
        for (int row = 0; row < boardSize; ++row) {
            for (int col = 0; col < boardSize; ++col) {
                const char piece = board[row][col];
                if (piece == '.') {
                    continue;
                }

                const int materialScore = getPieceValue(piece) * 10;
                const bool isWhite = std::isupper(static_cast<unsigned char>(piece)) != 0;

                int positionalScore = 0;

                const bool nearCenter = (row >= 2 && row <= 5 && col >= 2 && col <= 5);
                if (nearCenter) {
                    positionalScore += 1; // +0.1
                }

                if (std::tolower(static_cast<unsigned char>(piece)) == 'p') {
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
    };

    const auto getAllValidMoves = [&](bool isWhiteTurnForMoves) {
        std::vector<Move> moves;
        for (int fromRow = 0; fromRow < boardSize; ++fromRow) {
            for (int fromCol = 0; fromCol < boardSize; ++fromCol) {
                const char piece = board[fromRow][fromCol];
                if (piece == '.') {
                    continue;
                }

                const bool pieceIsWhite = std::isupper(static_cast<unsigned char>(piece)) != 0;
                if (pieceIsWhite != isWhiteTurnForMoves) {
                    continue;
                }

                for (int toRow = 0; toRow < boardSize; ++toRow) {
                    for (int toCol = 0; toCol < boardSize; ++toCol) {
                        if (!isValidMove(fromRow, fromCol, toRow, toCol)) {
                            continue;
                        }
                        moves.push_back(Move{fromRow, fromCol, toRow, toCol});
                    }
                }
            }
        }
        return moves;
    };

    const auto hasAnyValidMoves = [&](bool isWhite) {
        return !getAllValidMoves(isWhite).empty();
    };

    const auto printGameStateIfOver = [&](bool sideToMoveIsWhite) {
        if (hasAnyValidMoves(sideToMoveIsWhite)) {
            return false;
        }

        if (isKingInCheck(sideToMoveIsWhite)) {
            if (sideToMoveIsWhite) {
                std::cout << "Checkmate! Black wins\n";
            } else {
                std::cout << "Checkmate! White wins\n";
            }
        } else {
            std::cout << "Stalemate\n";
        }

        return true;
    };

    const auto makeMove = [&](const Move& move) {
        const char capturedPiece = board[move.toRow][move.toCol];
        board[move.toRow][move.toCol] = board[move.fromRow][move.fromCol];
        board[move.fromRow][move.fromCol] = '.';
        return capturedPiece;
    };

    const auto undoMove = [&](const Move& move, const char capturedPiece) {
        board[move.fromRow][move.fromCol] = board[move.toRow][move.toCol];
        board[move.toRow][move.toCol] = capturedPiece;
    };

    std::function<int(int, int, int, bool)> minimax = [&](int depth, int alpha, int beta, bool isMaximizing) -> int {
        if (depth == 0) {
            return evaluateBoard();
        }

        const std::vector<Move> moves = getAllValidMoves(isMaximizing);
        if (moves.empty()) {
            return evaluateBoard();
        }

        if (isMaximizing) {
            int bestScore = INT_MIN;
            for (const Move& move : moves) {
                const char capturedPiece = makeMove(move);
                const int score = minimax(depth - 1, alpha, beta, false);
                undoMove(move, capturedPiece);
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
            const char capturedPiece = makeMove(move);
            const int score = minimax(depth - 1, alpha, beta, true);
            undoMove(move, capturedPiece);
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
    };

    const auto getBestMove = [&](int depth) {
        const std::vector<Move> blackMoves = getAllValidMoves(false);
        Move bestMove{0, 0, 0, 0};
        int bestScore = INT_MAX;

        for (const Move& move : blackMoves) {
            const char capturedPiece = makeMove(move);
            const int score = minimax(depth - 1, INT_MIN, INT_MAX, true);
            undoMove(move, capturedPiece);

            if (score < bestScore) {
                bestScore = score;
                bestMove = move;
            }
        }

        return bestMove;
    };

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (gameOver) {
                    continue;
                }

                if (mousePressed->button != sf::Mouse::Button::Left) {
                    continue;
                }

                const int col = static_cast<int>(mousePressed->position.x) / squareSize;
                const int row = static_cast<int>(mousePressed->position.y) / squareSize;
                if (!isInsideBoard(row, col)) {
                    continue;
                }

                if (!hasSelection) {
                    const char piece = board[row][col];
                    if (piece == '.') {
                        continue;
                    }

                    const bool isWhitePiece = std::isupper(static_cast<unsigned char>(piece)) != 0;
                    if (isWhitePiece != whiteTurn) {
                        continue;
                    }

                    selectedRow = row;
                    selectedCol = col;
                    hasSelection = true;
                    std::cout << "Selected: (" << selectedRow << ", " << selectedCol << ") piece=" << piece << '\n';
                } else {
                    const char clickedPiece = board[row][col];
                    if (clickedPiece != '.') {
                        const bool clickedIsWhite = std::isupper(static_cast<unsigned char>(clickedPiece)) != 0;
                        if (clickedIsWhite == whiteTurn) {
                            selectedRow = row;
                            selectedCol = col;
                            std::cout << "Selected: (" << selectedRow << ", " << selectedCol
                                      << ") piece=" << clickedPiece << '\n';
                            continue;
                        }
                    }

                    if (!isValidMove(selectedRow, selectedCol, row, col)) {
                        std::cout << "Invalid move: (" << selectedRow << ", " << selectedCol
                                  << ") -> (" << row << ", " << col << ")\n";
                        continue;
                    }

                    const char movingPiece = board[selectedRow][selectedCol];
                    board[row][col] = movingPiece;
                    board[selectedRow][selectedCol] = '.';
                    hasLastMove = true;
                    lastMove = Move{selectedRow, selectedCol, row, col};
                    moveSound.play();

                    std::cout << "Move: (" << selectedRow << ", " << selectedCol << ") -> (" << row << ", " << col << ")\n";

                    hasSelection = false;
                    selectedRow = -1;
                    selectedCol = -1;
                    whiteTurn = !whiteTurn;

                    if (printGameStateIfOver(false)) {
                        gameOver = true;
                        continue;
                    }

                    if (!whiteTurn) {
                        const std::vector<Move> blackMoves = getAllValidMoves(false);
                        if (!blackMoves.empty()) {
                            const Move aiMove = getBestMove(3);
                            makeMove(aiMove);
                            hasLastMove = true;
                            lastMove = aiMove;
                            moveSound.play();
                            std::cout << "AI Move: (" << aiMove.fromRow << ", " << aiMove.fromCol
                                      << ") -> (" << aiMove.toRow << ", " << aiMove.toCol << ")\n";
                        }
                        whiteTurn = true;

                        if (printGameStateIfOver(true)) {
                            gameOver = true;
                        }
                    }
                }
            }
        }

        window.clear();

        sf::RectangleShape square(sf::Vector2f(static_cast<float>(squareSize), static_cast<float>(squareSize)));
        sf::RectangleShape highlight(sf::Vector2f(static_cast<float>(squareSize), static_cast<float>(squareSize)));
        const sf::Color selectedColor(80, 120, 255, 120);
        const sf::Color lastMoveColor(255, 215, 0, 90);
        const sf::Color validMoveColor(80, 220, 120, 110);
        const sf::Color checkColor(255, 0, 0, 100);

        const bool whiteInCheck = isKingInCheck(true);
        const bool blackInCheck = isKingInCheck(false);
        int whiteKingRow = -1;
        int whiteKingCol = -1;
        int blackKingRow = -1;
        int blackKingCol = -1;

        for (int row = 0; row < boardSize; ++row) {
            for (int col = 0; col < boardSize; ++col) {
                if (board[row][col] == 'K') {
                    whiteKingRow = row;
                    whiteKingCol = col;
                } else if (board[row][col] == 'k') {
                    blackKingRow = row;
                    blackKingCol = col;
                }
            }
        }

        for (int row = 0; row < boardSize; ++row) {
            for (int col = 0; col < boardSize; ++col) {
                const bool isLight = ((row + col) % 2 == 0);
                square.setFillColor(isLight ? lightSquare : darkSquare);
                square.setPosition(sf::Vector2f(static_cast<float>(col * squareSize), static_cast<float>(row * squareSize)));
                window.draw(square);

                const sf::Vector2f cellPosition(static_cast<float>(col * squareSize), static_cast<float>(row * squareSize));

                if (hasLastMove) {
                    const bool isFromSquare = (row == lastMove.fromRow && col == lastMove.fromCol);
                    const bool isToSquare = (row == lastMove.toRow && col == lastMove.toCol);
                    if (isFromSquare || isToSquare) {
                        highlight.setFillColor(lastMoveColor);
                        highlight.setPosition(cellPosition);
                        window.draw(highlight);
                    }
                }

                if (hasSelection && row == selectedRow && col == selectedCol) {
                    highlight.setFillColor(selectedColor);
                    highlight.setPosition(cellPosition);
                    window.draw(highlight);
                } else if (hasSelection && isValidMove(selectedRow, selectedCol, row, col)) {
                    highlight.setFillColor(validMoveColor);
                    highlight.setPosition(cellPosition);
                    window.draw(highlight);
                }

                const bool isWhiteKingInCheckSquare = whiteInCheck && row == whiteKingRow && col == whiteKingCol;
                const bool isBlackKingInCheckSquare = blackInCheck && row == blackKingRow && col == blackKingCol;
                if (isWhiteKingInCheckSquare || isBlackKingInCheckSquare) {
                    highlight.setFillColor(checkColor);
                    highlight.setPosition(cellPosition);
                    window.draw(highlight);
                }

                const char piece = board[row][col];
                if (piece == '.') {
                    continue;
                }

                const auto textureIt = pieceTextures.find(piece);
                if (textureIt == pieceTextures.end()) {
                    continue;
                }

                sf::Sprite sprite(textureIt->second);
                const sf::Vector2u textureSize = textureIt->second.getSize();
                if (textureSize.x > 0 && textureSize.y > 0) {
                    sprite.setScale(sf::Vector2f(
                        static_cast<float>(squareSize) / static_cast<float>(textureSize.x),
                        static_cast<float>(squareSize) / static_cast<float>(textureSize.y)
                    ));
                }
                sprite.setPosition(sf::Vector2f(static_cast<float>(col * squareSize), static_cast<float>(row * squareSize)));
                window.draw(sprite);
            }
        }

        window.display();
    }

    return 0;
}
