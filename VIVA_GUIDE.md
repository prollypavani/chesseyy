# Chesseyy Viva Guide (Competitive Programming Lab View)

This guide explains the full codebase in depth from a Competitive Programming (CP) lab perspective: what each file does, how the runtime flows, what algorithms/data structures are used, why those choices matter, their time complexity, and what can be asked in viva.

---

## 1) What this project is

`chesseyy` is a C++17 chess application with:
- SFML-based GUI (`Graphics`, `Window`, `System`, `Audio`)
- Human vs AI gameplay
- Legal move checking (with king safety)
- Check/checkmate/stalemate detection
- AI search with:
  - Plain minimax
  - Alpha-beta pruning
  - Move ordering (MVV-LVA style scoring)
  - Zobrist-hash transposition table
- Lab-oriented CP showcases:
  - Binary search on opening book
  - Prefix sums for pawn structure (passed pawns)
  - DSU/Union-Find for defensive component visualization
  - Topological-sort style capture-priority visualization

---

## 2) Build system and dependencies

### `CMakeLists.txt`
- Sets minimum CMake 3.16
- Project name: `chesseycpp`
- Uses C++17
- Requires SFML 3 with components: `Graphics`, `Window`, `System`, `Audio`
- Compiles:
  - `src/main.cpp`
  - `src/board.cpp`
  - `src/moves.cpp`
  - `src/engine.cpp`

### Build commands
```bash
cmake -S . -B build
cmake --build build
./build/chesseycpp
```

---

## 3) Module-by-module architecture

### A) `src/board.h` + `src/board.cpp`

#### Purpose
Defines board representation and initial board setup.

#### Core type
- `constexpr int BOARD_SIZE = 8;`
- `using Board = array<array<char, BOARD_SIZE>, BOARD_SIZE>;`

#### Piece encoding
- Uppercase = White: `P R N B Q K`
- Lowercase = Black: `p r n b q k`
- Empty square = `'.'`

#### Functions
- `createInitialBoard()`: returns standard starting chess position.
- `printBoard(const Board&)`: console print helper.

#### CP angle
- Uses fixed-size arrays (`std::array`) for cache-friendly, contiguous board representation.
- Deterministic and no dynamic allocation per board creation.

---

### B) `src/moves.h` + `src/moves.cpp`

This is the rules engine for movement legality and move generation.

#### `Move` struct
- 4 integers:
  - `fromRow`, `fromCol`, `toRow`, `toCol`

#### Utility functions
- `isInsideBoard(row, col)`
- `isPathClear(board, from, to)` for slider pieces (rook/bishop/queen)

#### Rule layers

1. **Pseudo-legal layer**: `isPseudoLegalMove(...)`
   - Enforces movement pattern, board bounds, own-piece capture prevention.
   - Piece-wise rules:
     - Pawn: 1-step, 2-step from start rank, diagonal capture.
     - Knight: L-shape.
     - Bishop: diagonal + clear path.
     - Rook: straight + clear path.
     - Queen: straight/diagonal + clear path.
     - King: one-square neighborhood.
   - Important: no castling/en-passant/promotion-specific logic here.

2. **Check layer**: `isKingInCheck(board, isWhite)`
   - Finds side’s king square.
   - Scans all opponent pieces and asks whether king square is pseudo-legally attacked.

3. **Full legal layer**: `isValidMove(board, from, to)`
   - First checks pseudo-legality.
   - Then simulates move on board.
   - Calls `isKingInCheck` for moving side.
   - Restores board and returns legality.

#### Move generation
- `getAllValidMoves(board, isWhiteTurnForMoves)`
  - Loops all `from` and all `to` squares (64x64 checks in worst case per side)
  - Filters via `isValidMove`
- `hasAnyValidMoves(...)` is convenience wrapper

#### State mutation helpers
- `makeMove(board, move)` returns captured piece char.
- `undoMove(board, move, capturedPiece)` restores previous state.

#### CP data structure showcase: DSU
- Globals:
  - `dsuParent[64]`, `dsuSize[64]`
- Methods:
  - `initDSU()`
  - `findDSU(x)` with path compression
  - `unionDSU(x, y)` union by size
  - `computeDefensiveComponents(board)`
- Concept:
  - Connects same-color pieces if one pseudo-legally “defends” another.
  - Used in UI visualization (hold `D`) to display piece-defense clusters.

---

### C) `src/engine.h` + `src/engine.cpp`

This is the AI evaluation + search core.

#### Global runtime counters/flags
- `int nodesExplored`
- `bool useAlphaBeta`
- `int ttHits`
- `vector<pair<int,int>> passedPawnsGlobal`

#### Piece values
- `P=1, N=3, B=3, R=5, Q=9, K=100`
- Evaluation multiplies material by 10 to work in decipawn-like scale.

#### Evaluation function: `evaluateBoard(board)`
Computes score from white perspective (positive = better for White):

1. Material term:
   - `pieceValue * 10`
2. Center bonus:
   - +1 if piece in central 4x4 region (`rows 2..5`, `cols 2..5`)
3. Pawn advancement bonus:
   - White pawn: `+ (6 - row)`
   - Black pawn: `+ (row - 1)` then subtracted due to black side
4. Passed pawn detection using prefix sums:
   - Build file-count arrays for white/black pawns
   - Build prefix arrays
   - Query adjacent files `[col-1, col+1]` in O(1) via `getRangeSum`
   - If no opposing pawn in these files -> passed pawn bonus `+30`
   - Save passed pawn coordinates in `passedPawnsGlobal` for green-ring UI

#### Move ordering
- `scoreMove(board, move)`:
  - If capture: `victimValue*10 - attackerValue` (MVV-LVA-like)
- `getOrderedMoves(board, side)`:
  - Generates all legal moves
  - Sorts descending by `scoreMove`
- Benefit:
  - Better alpha-beta cutoffs due to good ordering.

#### Search algorithms

1. `minimax_plain(board, depth, isMaximizing)`
   - Standard recursive minimax.
   - No pruning.
   - Uses ordered moves (still helps best-move stability).

2. `minimax_ab(board, depth, alpha, beta, isMaximizing)`
   - Alpha-beta pruning version.
   - Same recursive framework with cutoff checks.
   - Integrates transposition table.

#### Zobrist hashing + transposition table (TT)

##### Zobrist table
- `uint64_t zobristTable[8][8][12]`
- Initialized once with fixed RNG seed (`12345`) in `initZobrist()`
- Hash is XOR of piece-randoms at occupied squares.

##### TT structure
- `unordered_map<uint64_t, TTEntry>`
- `TTEntry = {depth, score, flag}`
- Flags:
  - `EXACT`
  - `LOWERBOUND`
  - `UPPERBOUND`

##### Lookup/store logic
- On hit with adequate stored depth:
  - exact -> direct return
  - lower/upper bound -> tighten alpha/beta
  - if window closes -> return cached score
- On store:
  - maximizing branch sets flag depending on relation with original window
  - minimizing branch similarly stores bound type

#### Best move selection for AI
- `getBestMove(board, depth)`
- AI is coded as Black (minimizer).
- For each candidate black move:
  - Make move
  - Evaluate child with chosen search mode (`useAlphaBeta`)
  - Undo move
  - Keep move with minimum score

#### CP showcase: Topological-sort style SEE print
- `printSEETopoSort(board, targetRow, targetCol)`
- Builds list of pieces pseudo-legally attacking target square.
- Builds DAG where lower-value attackers come earlier.
- Performs queue-based topo traversal and prints capture priority chain.
- Educational/visual feature; not feeding move search directly.

---

### D) `src/main.cpp`

This is integration glue: event loop + rendering + turn control + analytics display.

#### Startup pipeline
1. Initializes Zobrist (`initZobrist()`).
2. Creates initial board.
3. Loads piece textures.
4. Loads move sound (`assets/move.wav`).
5. Attempts font loading from multiple platform paths.
6. Creates SFML window and runtime state variables.

#### Opening book (`getOpeningBook()`)
- Returns vector of `(moveHistoryKey, Move)`.
- Keys are encoded move strings like `"6444:1434:..."`.
- Sorts book entries.
- At AI turn:
  - Uses `lower_bound` binary search on key.
  - If exact key found -> play book move instantly.
  - Else fall back to search.

#### Turn and input flow

##### Human (White) turn
- Click once: select own piece.
- Click second square:
  - If own piece -> reselection.
  - Else validate via `isValidMove`.
  - On valid move:
    - Update board
    - Append to `moveHistory`
    - Play sound
    - Optional SEE print if capture
    - Compute move quality:
      - `playedMoveEval` from resulting position
      - `bestMoveEval` by searching all legal white alternatives from pre-move board
      - classify using centipawn-loss thresholds:
        - <=20 Best
        - <=50 Good
        - <=100 Inaccuracy
        - <=300 Mistake
        - else Blunder
    - Switch side
    - Detect mate/stalemate

##### AI (Black) turn
- If opening book match -> use book move.
- Otherwise benchmark two methods:
  - plain minimax depth 3 (`useAlphaBeta=false`)
  - alpha-beta depth 3 (`useAlphaBeta=true`)
- Records:
  - nodes for minimax
  - nodes for alpha-beta
  - elapsed ms
  - transposition table hits
- Applies AI move, updates move history, switches side, checks game end.

#### Rendering
- Draw board squares.
- Draw overlays:
  - hover square
  - selected square
  - last move squares
  - check-highlight king square
  - valid move indicators (dot/ring style)
  - passed pawn green glow rings
  - optional DSU network graph when `D` pressed
- Draw side panel:
  - game status
  - CP analytics
  - TT hits
  - opening book status
  - minimax/alpha-beta node counts
  - estimated pruning savings
  - move quality and evals
  - short popup after move quality update

---

## 4) End-to-end runtime flow (important for viva)

1. Program starts -> board + assets + GUI initialized.
2. White selects and plays legal move.
3. Move legality:
   - pattern legality
   - then own king safety simulation
4. White move evaluated and classified.
5. Game-end check for black side.
6. Black move:
   - opening-book binary search attempt
   - else minimax/alpha-beta search
7. Apply AI move and check game-end for white side.
8. UI redraws every frame with current analytics.

---

## 5) Competitive Programming concepts mapped explicitly

### 1. Arrays / static memory
- Fixed 8x8 board with `std::array`.
- Constant-size DSU arrays of 64 nodes.

### 2. Binary search
- `lower_bound` on sorted opening book keys.
- Time: O(log n) lookup.

### 3. Prefix sums
- Pawn file counts and prefix arrays for O(1) range query.
- Used for passed-pawn detection.

### 4. Graph + topological processing
- SEE visualization models attacker ordering as DAG.
- Queue-based topological traversal.

### 5. DSU / Union-Find
- Defensive-component clustering with path compression + union by size.

### 6. Recursive game-tree search
- Minimax and alpha-beta.
- Backtracking via make/undo move.

### 7. Hashing
- Zobrist hashing to key board states.
- Transposition-table memoization.

### 8. Sorting + custom comparator
- Move ordering via `sort` and capture heuristic.

---

## 6) Complexity discussion (viva-ready)

Let `b` = branching factor (legal moves per position), `d` = search depth.

### Move validation / generation
- `isPseudoLegalMove`: O(1) for knight/king/pawn, O(k) path scan for sliders (`k <= 7`).
- `isValidMove`: pseudo-check + one simulated move + `isKingInCheck`.
- `isKingInCheck`: scans board and probes pseudo-legal attacks -> bounded on 8x8 but conceptually O(64 * move_check_cost).
- `getAllValidMoves`: loops every from-square and to-square (64*64) with legality checks.

### Search
- Plain minimax: O(b^d)
- Alpha-beta:
  - worst case still O(b^d)
  - best-case with ideal ordering approaches O(b^(d/2))
  - practical benefit visible in node counters shown by UI

### Opening book lookup
- O(log n) via binary search.

### Evaluation
- O(64) scan + O(8) prefix creation (small constants).

### DSU computation
- Outer loops over board pairs -> up to O(64*64) pseudo-check probes.
- DSU operations amortized near-constant (inverse Ackermann).

---

## 7) Chess-rule coverage and limitations

### Implemented
- Standard piece movement
- Captures
- Check legality (cannot leave own king in check)
- Checkmate/stalemate detection by move-existence + check state

### Not fully implemented
- Castling
- En passant
- Explicit promotion handling (no promotion UI/rule branch)
- Draw rules like 50-move, repetition, insufficient material

Viva tip: explicitly mention these as current scope boundaries, not bugs.

---

## 8) Important global/shared state (be able to explain)

- `Board board` in main loop is single source of current position.
- `whiteTurn` controls side to move.
- `nodesExplored`, `useAlphaBeta`, `ttHits` are engine-level globals.
- `transpositionTable` persists across searches in current run unless cleared.
- `moveHistory` string drives opening-book key lookup.
- `passedPawnsGlobal` is recomputed by evaluation and consumed by renderer.

---

## 9) Why make/undo is central

Search performance depends on cheap state transitions.
- `makeMove` mutates board in-place and returns captured piece.
- recursive call explores deeper position.
- `undoMove` restores exact parent state.

This avoids expensive full-board copy at each node (except where code intentionally copies for isolated calculations like move-quality benchmarking from pre-move board).

---

## 10) AI decision semantics (sign convention)

- Evaluation score positive => White advantage.
- Human is White, AI is Black.
- So AI (`getBestMove`) minimizes score.
- White move-quality grading compares:
  - eval after played move vs eval after best available white move
  - difference converted to centipawn-loss-like value.

---

## 11) Key viva questions with model answers

### Q1: Why two legality functions (`isPseudoLegalMove` and `isValidMove`)?
Because chess legality has two stages: movement pattern legality and king-safety legality. Pseudo-legal checks are reusable and fast; full legal validation simulates move and ensures own king is not in check.

### Q2: Why alpha-beta if minimax already works?
Alpha-beta gives the same optimal result while pruning branches that cannot influence final decision, reducing node expansions dramatically in practice.

### Q3: What does move ordering do?
It tries promising captures first, increasing chance of earlier alpha/beta cutoffs, thus improving effective branching factor.

### Q4: Why Zobrist hashing?
It maps board states to 64-bit keys fast; transposition table can reuse previously computed subtree scores when positions reoccur.

### Q5: Why prefix sums for passed pawns?
Need repeated “any enemy pawn in adjacent files?” queries. Prefix arrays turn each range query from O(k) scanning to O(1), while preprocessing is tiny.

### Q6: What is DSU contributing here?
It models support/defense connectivity among same-color pieces and visualizes strategic clusters, demonstrating graph connectivity ideas.

### Q7: Is topo sort truly needed for SEE?
In strict engine terms not necessary; here it is an educational CP representation to print an ordered attacker sequence.

### Q8: Why is board represented as chars, not objects?
Simple and fast. Char encoding keeps move generation compact and branch-friendly for CP-style implementation.

### Q9: What is the principal bottleneck?
Game-tree expansion (`getAllValidMoves` and recursive minimax/alpha-beta calls), not rendering.

### Q10: How can strength be improved?
Iterative deepening, quiescence search, piece-square tables, killer/history heuristics, proper draw rules, advanced move-generation optimizations, and better transposition-table management.

---

## 12) Potential viva “improvements” section

If examiner asks “what next?”:
- Add castling/en passant/promotion.
- Add FEN/PGN support.
- Add draw-rule detection.
- Add iterative deepening + aspiration windows.
- Add quiescence search to reduce horizon effect.
- Add opening book from real PGN.
- Add unit tests for legal move generation and mate/stalemate positions.

---

## 13) Practical explanation script (2-minute viva answer)

"The project is split into four modules: board representation, move legality, engine search, and SFML integration. `board.*` stores an 8x8 char board and initializes start position. `moves.*` handles pseudo-legal movement, king safety checks, and full legal move generation using simulation plus undo. `engine.*` scores positions with material and positional terms, then searches via minimax and alpha-beta, enhanced by move ordering and Zobrist-hash transposition table. `main.cpp` drives event handling, rendering, human move input, AI response, game-end checks, and analytics display. From a CP perspective, the code intentionally demonstrates binary search (opening book), prefix sums (passed pawn detection), DSU (defensive networks), graph/topological traversal (SEE-style capture ordering), sorting comparators, and recursive backtracking search."

---

## 14) File map quick revision

- `CMakeLists.txt`: build + SFML linkage
- `src/board.h` / `src/board.cpp`: board type + initial position + print
- `src/moves.h` / `src/moves.cpp`: legality rules, king safety, move gen, make/undo, DSU support graph
- `src/engine.h` / `src/engine.cpp`: evaluation, move ordering, minimax/plain + alpha-beta + TT + SEE topo output
- `src/main.cpp`: SFML app loop, input, rendering, opening book, benchmarking, status panel
- `README.md`: setup and high-level feature list
- `report.tex`: formal mini-project report source

---

## 15) Last-minute viva checklist

- Be clear on score sign (white-positive, black-minimizing).
- Be ready to explain pseudo-legal vs legal.
- Mention complexity with `b^d` and alpha-beta pruning impact.
- Explain why move ordering matters before alpha-beta.
- Explain TT flags (exact/lower/upper bound).
- State unimplemented rules confidently as scope limits.

---

If you want, I can also generate:
- a **one-page short viva cheat sheet** from this file, and/or
- a **faculty-style probable questions set** (easy/medium/hard tiers).
