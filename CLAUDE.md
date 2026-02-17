# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

NFC-based tabletop card game written in C11. Two players use physical NFC readers (3 per player) to play cards on a split-screen 1920x1080 display rendered with Raylib. Card data is stored in PostgreSQL and loaded as JSON.

## Build & Run

```bash
# Build
make cardgame                # Main game binary → ./cardgame
make preview                 # Card preview tool → ./card_preview

# Run (starts PostgreSQL via Docker, then runs)
docker compose up -d
make run                     # Cleans, builds, and runs with default DB connection

# Card preview (no DB needed)
make preview-run

# CMake (CLion)
cmake -B cmake-build-debug && cmake --build cmake-build-debug
```

Compiler: `gcc -Wall -Wextra -O2`. Links: `-lpq -lraylib -lm`. On macOS, Homebrew paths are added via `MACFLAGS`.

## Architecture

**Core loop** (`src/core/game.c`): init → update → render → cleanup. `GameState` (defined in `types.h`) owns all shared state: DB connection, card deck/atlas, both players, tileset, and sprite atlas.

**Split-screen**: The 1920x1080 screen is split into two 960x1080 viewports. Player 1 (left) has +90° camera rotation, Player 2 (right) has -90°. Each player has their own `Camera2D`, play area in world space, and screen area.

**Card pipeline**: PostgreSQL (`src/data/db.c`) → card deck loading (`src/data/cards.c`) → card effect registry (`src/logic/card_effects.c`) → visual rendering (`src/rendering/card_renderer.c`). Card visuals are parameterized via JSON with customizable colors, styles, and sprite offsets.

**Key modules**:
- `src/rendering/` — card sprites, tilemap generation (procedural), sprite animation, viewport management
- `src/systems/player.c` — player state, camera setup, card slot management
- `src/logic/card_effects.c` — callback registry mapping card types to `CardPlayFn` function pointers
- `src/data/` — PostgreSQL wrapper and card deck loading
- `src/entities/` — entity type system (troop, building, projectile) — largely stubbed
- `src/hardware/` — NFC reader and Arduino protocol — stubbed

**External libs** are in `lib/`: cJSON (compiled from source), raylib/raymath/rlgl headers, libpq header.

## Conventions

- **Naming**: functions are `module_action()` (e.g., `player_init`, `card_draw`), structs/enums are `CamelCase`, constants are `UPPER_SNAKE_CASE`
- **Include guards**: `#ifndef NFC_CARDGAME_MODULE_H`
- **Unused params**: cast to `(void)` to suppress warnings
- **Memory**: `malloc`/`free` with zero-init via `memset` or `{0}`
- **Config**: screen dimensions, asset paths, and gameplay tuning constants live in `src/core/config.h`
- **New source files** must be added to the `SOURCES` variable groups in the `Makefile` and to `CMakeLists.txt`

## Database

PostgreSQL 16 via Docker Compose. Backup at `postgres/appdb_2-16.backup`.

```bash
docker compose up -d         # Start PostgreSQL + pgAdmin
docker compose down           # Stop
```

Connection string: `host=localhost port=5432 dbname=appdb user=postgres password=postgres` (passed via `DB_CONNECTION` env var).
