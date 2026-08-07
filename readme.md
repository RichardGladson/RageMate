# ♟️ RageMate Maxima V2

*A standalone ESP32-powered handheld chess computer featuring a custom chess engine, dual-core search, and a full graphical interface.*

---

## Overview

**RageMate Maxima V2** is a self-contained chess computer built around the ESP32. Unlike projects that stream moves from a PC or rely on online engines, the entire chess engine runs directly on the microcontroller.

The engine implements full legal chess rules, an optimized alpha-beta search, handcrafted evaluation, an opening book, and a responsive user interface running simultaneously on the ESP32's dual-core architecture.

---

## Features

### ♟ Chess Engine

- Complete legal move generation
- Castling, en passant, promotions
- Check, pin, and legality validation
- Incremental 32-bit Zobrist hashing
- 4096-entry transposition table
- Repetition detection
- Insufficient material detection
- Undo stack supporting 300 half-moves

---

### 🔍 Search

The engine uses an optimized **Negamax Alpha-Beta** search with iterative deepening.

Implemented search techniques include:

- Iterative Deepening
- Alpha-Beta Pruning
- Aspiration Windows
- Transposition Table Cutoffs
- Null Move Pruning
- Late Move Reductions (LMR)
- Killer Move Heuristic
- History Heuristic
- MVV/LVA Move Ordering
- Quiescence Search
- Static Exchange Evaluation (SEE)
- Delta Pruning
- Futility Pruning

Additional custom heuristics include:

- Knight pawn-kick prediction during the opening
- Anti-sacrifice verification after reduced searches
- Horizon-effect guard that compares shallower search results before committing to the deepest move

Search runs until:

- **Depth 9**
- or
- **5 second time limit**

whichever comes first.

---

### 📊 Evaluation

The evaluation function combines material, positional, and strategic factors.

#### Material

| Piece | Value |
|------|------:|
| Pawn | 100 |
| Knight | 320 |
| Bishop | 330 |
| Rook | 500 |
| Queen | 900 |
| King | 20000 |

#### Positional Evaluation

- Piece-Square Tables
- Separate middlegame/endgame king tables
- Mobility scoring
- Bishop pair bonus
- Passed pawn bonuses
- Doubled pawn penalties
- Isolated pawn penalties
- Backward pawn penalties
- Open and semi-open file evaluation
- Rook activity bonuses
- Endgame king centralization
- Pawn threat detection

Opening-specific heuristics include:

- Undeveloped minor penalties
- Rim knight penalties
- Premature queen development penalties
- Castling rights bonus

---

### 📚 Opening Book

The engine contains a handcrafted opening book covering Black's first three moves.

Unlike static lookup tables, every book move is:

- Verified against the current legal move list
- Passed through Static Exchange Evaluation
- Abandoned immediately if tactical opportunities exist

This allows the engine to leave the opening book whenever calculation becomes more valuable than memorized theory.

---

## Hardware

RageMate Maxima V2 is designed as a dedicated handheld chess computer.

### Microcontroller

- ESP32
- 240 MHz Dual-Core CPU

### Display

- Adafruit ST7735 SPI TFT
- 27 MHz SPI
- Rotation: 180°
- 8×8 board rendered using 13×13 pixel squares

### Controls

- Analog joystick
- Pushbutton
- Adjustable joystick sensitivity
- Full menu navigation without a keyboard or touchscreen

---

## GPIO Connections

| Function | GPIO |
|----------|----:|
| TFT SCLK | 18 |
| TFT MOSI | 23 |
| TFT DC | 16 |
| TFT RESET | 17 |
| TFT CS | 5 |
| Joystick X | 34 |
| Joystick Y | 35 |
| Joystick Button | 32 |

---

## Dual-Core Architecture

The ESP32's two CPU cores are used independently.

### Core 0

Runs the chess engine.

- Alpha-Beta search
- Move generation
- Evaluation
- Transposition table
- Search progress updates

### Core 1

Handles the interface.

- Display rendering
- Joystick polling
- Button input
- Menus
- Progress bar
- Depth display

This architecture keeps the interface responsive even while the engine is searching.

---

## Memory Optimizations

Designed specifically for the ESP32's limited SRAM.

- Fixed-size 4096-entry transposition table
- 300 half-move history stack
- Theme data stored in PROGMEM
- Incremental hashing
- Fixed-size engine structures

---

## User Interface

The project includes a fully custom graphical interface featuring:

- Animated splash screen
- Live search progress
- Current depth indicator
- Menu system
- Game-over screens
- Settings menu
- Runtime theme switching

### Included Themes

The firmware contains **38 built-in color themes**, including:

- Wood
- Cyberpunk
- Hacker
- Vampire
- Neon
- Retro
- Dark
- And many more

---

## Boot Process

On startup the firmware:

1. Performs a hardware TFT reset
2. Initializes SPI
3. Starts the display
4. Initializes joystick input
5. Generates Zobrist keys
6. Clears the transposition table
7. Plays the splash animation
8. Creates the initial board
9. Enters the main UI loop

---

## Technical Summary

| Specification | Value |
|--------------|------|
| Platform | ESP32 |
| Clock Speed | 240 MHz |
| Search Algorithm | Negamax Alpha-Beta |
| Maximum Search Depth | 9 plies |
| Time Limit | 5 seconds |
| Hashing | 32-bit Zobrist |
| Transposition Table | 4096 entries |
| Search Threads | Dual-Core FreeRTOS |
| Display | ST7735 TFT |
| Controls | Analog Joystick |
| Themes | 38 |

---

## Future Improvements

Potential future additions include:

- Stronger evaluation tuning
- Larger opening book
- Endgame tablebase support
- Configurable difficulty levels
- PGN import/export
- Move logging
- Opening statistics
- Engine-versus-engine mode

---

## License

This project is released under the MIT License.

---

## Author

Developed by **Richard** as a custom embedded chess computer combining low-level firmware development, embedded graphics, and chess engine programming.