# Chess Engine (SFML + Minimax)

This project is a C++ chess engine with an SFML user interface.

## Features

- 8x8 board rendering with textures and move sound
- Click-to-select and click-to-move interaction
- Legal move generation for all pieces
- Check, checkmate, and stalemate detection
- AI opponent using minimax and alpha-beta pruning
- Minimax vs alpha-beta benchmarking output
- Move quality classification:
  - Best Move
  - Good Move
  - Inaccuracy
  - Mistake
  - Blunder
- UI feedback:
  - selected square highlight
  - legal move highlights
  - last move highlight
  - king-in-check warning
  - hover highlight
  - side info panel

## Project Structure

- `src/main.cpp` - SFML loop, input handling, UI rendering, integration
- `src/board.h`, `src/board.cpp` - board representation and initialization
- `src/moves.h`, `src/moves.cpp` - move validation and board move helpers
- `src/engine.h`, `src/engine.cpp` - evaluation and minimax/alpha-beta engine
- `CMakeLists.txt` - build configuration

## Build and Run

From project root:

```bash
cmake -S . -B build
cmake --build build
./build/chesseycpp
```

## Assets

Place these files in `assets/`:

- Piece textures:
  - `wp.png`, `wr.png`, `wn.png`, `wb.png`, `wq.png`, `wk.png`
  - `bp.png`, `br.png`, `bn.png`, `bb.png`, `bq.png`, `bk.png`
- Sound:
  - `move.wav`
- Optional font (if present, UI text uses it):
  - `DejaVuSans.ttf` or `arial.ttf`
