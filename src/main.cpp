#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <climits>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
using namespace std;

#include "board.h"
#include "engine.h"
#include "moves.h"

int main() {
    constexpr int boardPixelSize = 640;
    constexpr int sidePanelWidth = 280;
    constexpr int windowWidth = boardPixelSize + sidePanelWidth;
    constexpr int windowHeight = boardPixelSize;
    constexpr int squareSize = boardPixelSize / BOARD_SIZE;

    Board board = createInitialBoard();
    printBoard(board);

    const sf::Color lightSquare(240, 217, 181);
    const sf::Color darkSquare(181, 136, 99);

    const unordered_map<char, string> pieceFiles = {
        {'P', "assets/wp.png"}, {'R', "assets/wr.png"}, {'N', "assets/wn.png"},
        {'B', "assets/wb.png"}, {'Q', "assets/wq.png"}, {'K', "assets/wk.png"},
        {'p', "assets/bp.png"}, {'r', "assets/br.png"}, {'n', "assets/bn.png"},
        {'b', "assets/bb.png"}, {'q', "assets/bq.png"}, {'k', "assets/bk.png"}
    };

    unordered_map<char, sf::Texture> pieceTextures;
    for (const auto& [piece, path] : pieceFiles) {
        sf::Texture texture;
        if (!texture.loadFromFile(path)) {
            cerr << "Failed to load texture: " << path << '\n';
            return 1;
        }
        pieceTextures.emplace(piece, static_cast<sf::Texture&&>(texture));
    }

    sf::SoundBuffer moveBuffer;
    if (!moveBuffer.loadFromFile("assets/move.wav")) {
        cerr << "Failed to load sound: assets/move.wav\n";
        return 1;
    }
    sf::Sound moveSound(moveBuffer);

    sf::Font uiFont;
    bool hasFont = false;
    const array<string, 3> fontCandidates = {
        "assets/DejaVuSans.ttf",
        "assets/arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf"
    };
    for (const string& fontPath : fontCandidates) {
        if (uiFont.openFromFile(fontPath)) {
            hasFont = true;
            break;
        }
    }

    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(windowWidth, windowHeight)),
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
    string gameStatusText = "White to move";
    string lastMoveQualityText = "N/A";
    int lastPlayedEval = 0;
    int lastBestEval = 0;
    auto moveQualityPopupUntil = chrono::steady_clock::now();

    const auto printGameStateIfOver = [&](const bool sideToMoveIsWhite) {
        if (hasAnyValidMoves(board, sideToMoveIsWhite)) {
            return false;
        }

        if (isKingInCheck(board, sideToMoveIsWhite)) {
            if (sideToMoveIsWhite) {
                cout << "Checkmate! Black wins\n";
                gameStatusText = "Checkmate! Black wins";
            } else {
                cout << "Checkmate! White wins\n";
                gameStatusText = "Checkmate! White wins";
            }
        } else {
            cout << "Stalemate\n";
            gameStatusText = "Stalemate";
        }

        return true;
    };

    while (window.isOpen()) {
        while (const optional event = window.pollEvent()) {
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

                    const bool isWhitePiece = isupper(static_cast<unsigned char>(piece)) != 0;
                    if (isWhitePiece != whiteTurn) {
                        continue;
                    }

                    selectedRow = row;
                    selectedCol = col;
                    hasSelection = true;
                    cout << "Selected: (" << selectedRow << ", " << selectedCol << ") piece=" << piece << '\n';
                } else {
                    const char clickedPiece = board[row][col];
                    if (clickedPiece != '.') {
                        const bool clickedIsWhite = isupper(static_cast<unsigned char>(clickedPiece)) != 0;
                        if (clickedIsWhite == whiteTurn) {
                            selectedRow = row;
                            selectedCol = col;
                            cout << "Selected: (" << selectedRow << ", " << selectedCol
                                      << ") piece=" << clickedPiece << '\n';
                            continue;
                        }
                    }

                    if (!isValidMove(board, selectedRow, selectedCol, row, col)) {
                        cout << "Invalid move: (" << selectedRow << ", " << selectedCol
                                  << ") -> (" << row << ", " << col << ")\n";
                        continue;
                    }

                    const char movingPiece = board[selectedRow][selectedCol];
                    const Board boardBeforeMove = board;
                    board[row][col] = movingPiece;
                    board[selectedRow][selectedCol] = '.';
                    const Board boardAfterMove = board;
                    hasLastMove = true;
                    lastMove = Move{selectedRow, selectedCol, row, col};
                    moveSound.play();

                    cout << "Move: (" << selectedRow << ", " << selectedCol << ") -> (" << row << ", " << col << ")\n";

                    constexpr int qualityDepth = 3;
                    const auto evaluateAfterWhiteMove = [&](Board& positionAfterWhiteMove) {
                        return minimax_ab(positionAfterWhiteMove, qualityDepth - 1, INT_MIN, INT_MAX, false);
                    };

                    Board playedPosition = boardAfterMove;
                    const int playedMoveEval = evaluateAfterWhiteMove(playedPosition);

                    Board bestSearchPosition = boardBeforeMove;
                    const vector<Move> whiteMoves = getAllValidMoves(bestSearchPosition, true);
                    int bestMoveEval = playedMoveEval;
                    if (!whiteMoves.empty()) {
                        bestMoveEval = INT_MIN;
                        for (const Move& candidateMove : whiteMoves) {
                            Board candidatePosition = boardBeforeMove;
                            makeMove(candidatePosition, candidateMove);
                            const int candidateEval = evaluateAfterWhiteMove(candidatePosition);
                            if (candidateEval > bestMoveEval) {
                                bestMoveEval = candidateEval;
                            }
                        }
                    }

                    const int centipawnLoss = max(0, (bestMoveEval - playedMoveEval) * 10);
                    string moveQuality = "Blunder";
                    if (centipawnLoss <= 20) {
                        moveQuality = "Best Move";
                    } else if (centipawnLoss <= 50) {
                        moveQuality = "Good Move";
                    } else if (centipawnLoss <= 100) {
                        moveQuality = "Inaccuracy";
                    } else if (centipawnLoss <= 300) {
                        moveQuality = "Mistake";
                    }
                    lastMoveQualityText = moveQuality;
                    lastPlayedEval = playedMoveEval;
                    lastBestEval = bestMoveEval;
                    moveQualityPopupUntil = chrono::steady_clock::now() + chrono::seconds(2);

                    cout << fixed << setprecision(2);
                    cout << "You played:(" << selectedRow << "," << selectedCol << ")->(" << row << "," << col << ")\n";
                    cout << "Engine Evaluation: "
                              << (playedMoveEval >= 0 ? "+" : "") << (static_cast<double>(playedMoveEval) / 10.0) << '\n';
                    cout << "Best Move Evaluation: "
                              << (bestMoveEval >= 0 ? "+" : "") << (static_cast<double>(bestMoveEval) / 10.0) << '\n';
                    cout << "Move Quality: " << moveQuality << '\n';

                    hasSelection = false;
                    selectedRow = -1;
                    selectedCol = -1;
                    whiteTurn = !whiteTurn;
                    gameStatusText = "Black to move";

                    if (printGameStateIfOver(false)) {
                        gameOver = true;
                        continue;
                    }

                    if (!whiteTurn) {
                        const vector<Move> blackMoves = getAllValidMoves(board, false);
                        if (!blackMoves.empty()) {
                            nodesExplored = 0;
                            useAlphaBeta = false;
                            const auto startMinimax = chrono::high_resolution_clock::now();
                            const Move plainMove = getBestMove(board, 3);
                            (void)plainMove;
                            const auto endMinimax = chrono::high_resolution_clock::now();
                            const auto minimaxDuration = chrono::duration_cast<chrono::milliseconds>(endMinimax - startMinimax);
                            const int nodesMinimax = nodesExplored;

                            nodesExplored = 0;
                            useAlphaBeta = true;
                            const auto startAlphaBeta = chrono::high_resolution_clock::now();
                            const Move aiMove = getBestMove(board, 3);
                            const auto endAlphaBeta = chrono::high_resolution_clock::now();
                            const auto alphaBetaDuration = chrono::duration_cast<chrono::milliseconds>(endAlphaBeta - startAlphaBeta);
                            const int nodesAlphaBeta = nodesExplored;

                            makeMove(board, aiMove);
                            hasLastMove = true;
                            lastMove = aiMove;
                            moveSound.play();
                            cout << "AI Move: (" << aiMove.fromRow << ", " << aiMove.fromCol
                                      << ") -> (" << aiMove.toRow << ", " << aiMove.toCol << ")\n";
                            cout << "\n--- Benchmark ---\n";
                            cout << "Minimax: Nodes = " << nodesMinimax
                                      << ", Time = " << minimaxDuration.count() << " ms\n";
                            cout << "Alpha-Beta: Nodes = " << nodesAlphaBeta
                                      << ", Time = " << alphaBetaDuration.count() << " ms\n";
                        }
                        whiteTurn = true;
                        gameStatusText = "White to move";

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
        const sf::Color hoverColor(255, 255, 255, 45);

        const sf::Vector2i mousePosition = sf::Mouse::getPosition(window);
        const int hoverCol = mousePosition.x / squareSize;
        const int hoverRow = mousePosition.y / squareSize;
        const bool hasHover = mousePosition.x >= 0 && mousePosition.x < boardPixelSize &&
                              mousePosition.y >= 0 && mousePosition.y < boardPixelSize &&
                              isInsideBoard(hoverRow, hoverCol);

        const bool whiteInCheck = isKingInCheck(board, true);
        const bool blackInCheck = isKingInCheck(board, false);
        if (!gameOver) {
            if (whiteInCheck && whiteTurn) {
                gameStatusText = "White to move (Check)";
            } else if (blackInCheck && !whiteTurn) {
                gameStatusText = "Black to move (Check)";
            }
        }
        int whiteKingRow = -1;
        int whiteKingCol = -1;
        int blackKingRow = -1;
        int blackKingCol = -1;

        for (int row = 0; row < BOARD_SIZE; ++row) {
            for (int col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] == 'K') {
                    whiteKingRow = row;
                    whiteKingCol = col;
                } else if (board[row][col] == 'k') {
                    blackKingRow = row;
                    blackKingCol = col;
                }
            }
        }

        for (int row = 0; row < BOARD_SIZE; ++row) {
            for (int col = 0; col < BOARD_SIZE; ++col) {
                const bool isLight = ((row + col) % 2 == 0);
                square.setFillColor(isLight ? lightSquare : darkSquare);
                square.setPosition(sf::Vector2f(static_cast<float>(col * squareSize), static_cast<float>(row * squareSize)));
                window.draw(square);

                const sf::Vector2f cellPosition(static_cast<float>(col * squareSize), static_cast<float>(row * squareSize));

                if (hasHover && row == hoverRow && col == hoverCol) {
                    highlight.setFillColor(hoverColor);
                    highlight.setPosition(cellPosition);
                    window.draw(highlight);
                }

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
                } else if (hasSelection && isValidMove(board, selectedRow, selectedCol, row, col)) {
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

        const float panelX = static_cast<float>(boardPixelSize);
        sf::RectangleShape sidePanel(sf::Vector2f(static_cast<float>(sidePanelWidth), static_cast<float>(windowHeight)));
        sidePanel.setPosition(sf::Vector2f(panelX, 0.0f));
        sidePanel.setFillColor(sf::Color(30, 30, 35, 235));
        window.draw(sidePanel);

        if (hasFont) {
            auto drawPanelText = [&](const string& text, float x, float y, unsigned int size, sf::Color color) {
                sf::Text uiText(uiFont, text, size);
                uiText.setPosition(sf::Vector2f(x, y));
                uiText.setFillColor(color);
                window.draw(uiText);
            };

            drawPanelText("Game Status", panelX + 16.0f, 16.0f, 22, sf::Color(240, 240, 240));
            drawPanelText(gameStatusText, panelX + 16.0f, 48.0f, 18, sf::Color(220, 220, 220));

            drawPanelText("Move Quality", panelX + 16.0f, 96.0f, 20, sf::Color(240, 240, 240));
            drawPanelText(lastMoveQualityText, panelX + 16.0f, 126.0f, 18, sf::Color(230, 230, 140));

            drawPanelText(
                "Engine Eval: " + string(lastPlayedEval >= 0 ? "+" : "") +
                to_string(static_cast<double>(lastPlayedEval) / 10.0),
                panelX + 16.0f, 166.0f, 17, sf::Color(220, 220, 220)
            );
            drawPanelText(
                "Best Eval: " + string(lastBestEval >= 0 ? "+" : "") +
                to_string(static_cast<double>(lastBestEval) / 10.0),
                panelX + 16.0f, 192.0f, 17, sf::Color(220, 220, 220)
            );

            const bool showMoveQualityPopup = chrono::steady_clock::now() < moveQualityPopupUntil;
            if (showMoveQualityPopup) {
                sf::RectangleShape popup(sf::Vector2f(230.0f, 44.0f));
                popup.setPosition(sf::Vector2f(12.0f, 12.0f));
                popup.setFillColor(sf::Color(20, 20, 20, 180));
                window.draw(popup);
                drawPanelText("Move Quality: " + lastMoveQualityText, 20.0f, 22.0f, 20, sf::Color(245, 245, 160));
            }
        }

        window.display();
    }

    return 0;
}
