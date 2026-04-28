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

// --- CP Component: Array Techniques & Binary Search (Opening Book) ---
vector<pair<string, Move>> getOpeningBook() {
    vector<pair<string, Move>> book = {
        {"", {6, 4, 4, 4}},                                // e4
        {"6444:", {1, 4, 3, 4}},                           // e5
        {"6444:1434:", {7, 6, 5, 5}},                      // Nf3
        {"6444:1434:7655:", {0, 1, 2, 2}},                 // Nc6
        {"6444:1434:7655:0122:", {7, 5, 4, 2}},            // Bc4 (Italian)
        {"6444:1434:7655:0122:7542:", {0, 5, 3, 2}},       // Bc5
        {"6333:", {1, 3, 3, 3}},                           // d4 -> d5
    };
    // Ensure it's sorted for binary search
    std::sort(book.begin(), book.end(), [](const pair<string, Move>& a, const pair<string, Move>& b) {
        return a.first < b.first;
    });
    return book;
}

int main() {
    constexpr int boardPixelSize = 640;
    constexpr int sidePanelWidth = 340;
    constexpr int windowWidth = boardPixelSize + sidePanelWidth;
    constexpr int windowHeight = boardPixelSize;
    constexpr int squareSize = boardPixelSize / BOARD_SIZE;

    initZobrist();
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
    const array<string, 8> fontCandidates = {
        "assets/DejaVuSans.ttf",
        "assets/arial.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "C:/Windows/Fonts/arial.ttf"
    };
    for (const string& fontPath : fontCandidates) {
        std::error_code ec;
        if (!filesystem::exists(fontPath, ec)) {
            continue;
        }
        if (uiFont.openFromFile(fontPath)) {
            hasFont = true;
            break;
        }
    }

    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(windowWidth, windowHeight)),
        "Chessey - CP Enhanced"
    );
    window.setFramerateLimit(60);

    bool whiteTurn = true;
    bool hasSelection = false;
    string moveHistory = "";
    vector<pair<string, Move>> openingBook = getOpeningBook();
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
    
    // CP Metrics
    int currentMinimaxNodes = 0;
    int currentAlphaBetaNodes = 0;
    bool lastMoveWasBook = false;

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

                const sf::Vector2i clickPixels = sf::Mouse::getPosition(window);
                const sf::Vector2f clickWorld = window.mapPixelToCoords(clickPixels);
                const int col = static_cast<int>(clickWorld.x) / squareSize;
                const int row = static_cast<int>(clickWorld.y) / squareSize;
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
                    moveHistory += to_string(selectedRow) + to_string(selectedCol) + to_string(row) + to_string(col) + ":";
                    
                    // --- CP Trigger: TopoSort SEE
                    if (boardBeforeMove[row][col] != '.') {
                        printSEETopoSort(boardBeforeMove, row, col);
                    }

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
                            Move aiMove{0,0,0,0};
                            bool bookMoveFound = false;

                            // Binary Search for Opening Book Move
                            auto bookIt = std::lower_bound(openingBook.begin(), openingBook.end(), moveHistory,
                                [](const pair<string, Move>& entry, const string& history) {
                                    return entry.first < history;
                                });

                            if (bookIt != openingBook.end() && bookIt->first == moveHistory) {
                                aiMove = bookIt->second;
                                bookMoveFound = true;
                                lastMoveWasBook = true;
                                cout << "\n--- AI Move from Opening Book (Binary Search) ---\n";
                            }

                            if (!bookMoveFound) {
                                lastMoveWasBook = false;
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
                                aiMove = getBestMove(board, 3);
                                const auto endAlphaBeta = chrono::high_resolution_clock::now();
                                const auto alphaBetaDuration = chrono::duration_cast<chrono::milliseconds>(endAlphaBeta - startAlphaBeta);
                                const int nodesAlphaBeta = nodesExplored;
                                
                                currentMinimaxNodes = nodesMinimax;
                                currentAlphaBetaNodes = nodesAlphaBeta;
                                
                                cout << "\n--- Benchmark ---\n";
                                cout << "Minimax: Nodes = " << nodesMinimax << ", Time = " << minimaxDuration.count() << " ms\n";
                                cout << "Alpha-Beta: Nodes = " << nodesAlphaBeta << ", Time = " << alphaBetaDuration.count() << " ms\n";
                                cout << "Transposition Table (Zobrist) Hits: " << ttHits << "\n";
                            }
                            
                            moveHistory += to_string(aiMove.fromRow) + to_string(aiMove.fromCol) + to_string(aiMove.toRow) + to_string(aiMove.toCol) + ":";
                            makeMove(board, aiMove);
                            hasLastMove = true;
                            lastMove = aiMove;
                            moveSound.play();
                            cout << "AI Move: (" << aiMove.fromRow << ", " << aiMove.fromCol
                                      << ") -> (" << aiMove.toRow << ", " << aiMove.toCol << ")\n";
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

        const sf::Vector2i mousePixels = sf::Mouse::getPosition(window);
        const sf::Vector2f mouseWorld = window.mapPixelToCoords(mousePixels);
        const int hoverCol = static_cast<int>(mouseWorld.x) / squareSize;
        const int hoverRow = static_cast<int>(mouseWorld.y) / squareSize;
        const bool hasHover = mouseWorld.x >= 0.0f && mouseWorld.x < static_cast<float>(boardPixelSize) &&
                              mouseWorld.y >= 0.0f && mouseWorld.y < static_cast<float>(boardPixelSize) &&
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
                
                // Draw Valid Move Dots over pieces (like modern chess UIs)
                if (hasSelection && isValidMove(board, selectedRow, selectedCol, row, col)) {
                    sf::CircleShape validDot(squareSize / 6.0f);
                    if (board[row][col] == '.') {
                        validDot.setFillColor(sf::Color(0, 0, 0, 40));
                        validDot.setPosition(sf::Vector2f((col + 0.5f) * squareSize - squareSize / 6.0f, (row + 0.5f) * squareSize - squareSize / 6.0f));
                    } else {
                        validDot.setFillColor(sf::Color(0, 0, 0, 0));
                        validDot.setOutlineColor(sf::Color(0, 0, 0, 50));
                        validDot.setOutlineThickness(7.0f);
                        validDot.setRadius(squareSize / 2.3f);
                        validDot.setPosition(sf::Vector2f((col + 0.5f) * squareSize - squareSize / 2.3f, (row + 0.5f) * squareSize - squareSize / 2.3f));
                    }
                    window.draw(validDot);
                }
                
                // Draw Passed Pawn Glowing Ring (Prefix Array Output)
                for (const auto& pawn : passedPawnsGlobal) {
                    if (pawn.first == row && pawn.second == col) {
                        sf::CircleShape glow(squareSize / 2.2f);
                        glow.setFillColor(sf::Color::Transparent);
                        glow.setOutlineColor(sf::Color(50, 255, 50, 150));
                        glow.setOutlineThickness(3.0f);
                        glow.setPosition(sf::Vector2f((col + 0.5f) * squareSize - squareSize / 2.2f, (row + 0.5f) * squareSize - squareSize / 2.2f));
                        window.draw(glow);
                    }
                }
            }
        }
        
        // --- CP Component: Draw DSU Defensive Groups
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
            computeDefensiveComponents(board);
            for (int r = 0; r < BOARD_SIZE; ++r) {
                for (int c = 0; c < BOARD_SIZE; ++c) {
                    if (board[r][c] != '.') {
                        int root = findDSU(r * 8 + c);
                        if (dsuSize[root] > 1) {
                            // Draw DSU Network Lines
                            for (int i = 0; i < 64; ++i) {
                                if (i > r * 8 + c && board[i/8][i%8] != '.' && findDSU(i) == root) {
                                    sf::Vertex line[2];
                                    line[0].position = sf::Vector2f((c + 0.5f) * squareSize, (r + 0.5f) * squareSize);
                                    line[1].position = sf::Vector2f((i%8 + 0.5f) * squareSize, (i/8 + 0.5f) * squareSize);
                                    int hashColor = (root * 137) % 360;
                                    sf::Color lineColor(50 + hashColor % 150, 100 + hashColor % 155, 200, 150);
                                    line[0].color = lineColor;
                                    line[1].color = lineColor;
                                    window.draw(line, 2, sf::PrimitiveType::Lines);
                                }
                            }
                            // Draw Node Dot
                            sf::CircleShape dsuDot(squareSize / 8.0f);
                            dsuDot.setPosition(sf::Vector2f((c + 0.5f) * squareSize - squareSize / 8.0f, (r + 0.5f) * squareSize - squareSize / 8.0f));
                            int hashColor = (root * 137) % 360;
                            dsuDot.setFillColor(sf::Color(50 + hashColor % 150, 100 + hashColor % 155, 200, 255));
                            window.draw(dsuDot);
                        }
                    }
                }
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
            
            drawPanelText(string("Hold 'D' for DSU Net"), panelX + 16.0f, 85.0f, 15, sf::Color(150, 200, 255));
            drawPanelText(string("Green rings = Passed Pawns"), panelX + 16.0f, 105.0f, 15, sf::Color(150, 255, 150));

            // CP Analytics Panel
            drawPanelText("CP Analytics", panelX + 16.0f, 140.0f, 20, sf::Color(255, 215, 0));
            
            sf::RectangleShape separator(sf::Vector2f(sidePanelWidth - 32.0f, 2.0f));
            separator.setPosition(sf::Vector2f(panelX + 16.0f, 170.0f));
            separator.setFillColor(sf::Color(100, 100, 100));
            window.draw(separator);

            drawPanelText(string("Zobrist TT Hits: ") + to_string(ttHits), panelX + 16.0f, 180.0f, 16, sf::Color(200, 200, 200));
            
            string bookText = lastMoveWasBook ? "Active (Binary Search)" : "Miss (Calculated)";
            drawPanelText(string("Opening Book: ") + bookText, panelX + 16.0f, 210.0f, 16, 
                lastMoveWasBook ? sf::Color(100, 255, 100) : sf::Color(200, 150, 150));

            drawPanelText("Nodes Evaluated: ", panelX + 16.0f, 240.0f, 16, sf::Color(200, 200, 200));
            drawPanelText(string("Minimax: ") + to_string(currentMinimaxNodes), panelX + 26.0f, 260.0f, 14, sf::Color(180, 180, 180));
            drawPanelText(string("Alpha-Beta: ") + to_string(currentAlphaBetaNodes), panelX + 26.0f, 280.0f, 14, sf::Color(180, 255, 180));
            
            if (currentMinimaxNodes > 0) {
                int saved = currentMinimaxNodes - currentAlphaBetaNodes;
                int percent = (saved * 100) / max(1, currentMinimaxNodes);
                drawPanelText(string("Ordering Saved: ") + to_string(percent) + "%", panelX + 26.0f, 300.0f, 14, sf::Color(255, 200, 100));
            }

            drawPanelText("Move Quality", panelX + 16.0f, 350.0f, 20, sf::Color(240, 240, 240));
            drawPanelText(lastMoveQualityText, panelX + 16.0f, 380.0f, 18, sf::Color(230, 230, 140));

            drawPanelText(
                "Engine Eval: " + string(lastPlayedEval >= 0 ? "+" : "") +
                to_string(static_cast<double>(lastPlayedEval) / 10.0),
                panelX + 16.0f, 420.0f, 17, sf::Color(220, 220, 220)
            );
            drawPanelText(
                "Best Eval: " + string(lastBestEval >= 0 ? "+" : "") +
                to_string(static_cast<double>(lastBestEval) / 10.0),
                panelX + 16.0f, 446.0f, 17, sf::Color(220, 220, 220)
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
