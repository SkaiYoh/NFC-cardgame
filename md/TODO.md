# NFC Card Game - TODO

Last verified: 2026-04-04

## Verification Snapshot

- `make test` passes:
  - pathfinding: 6 tests
  - combat: 29 tests
  - battlefield_math: 12 tests
  - battlefield: 12 tests
  - animation: 28 tests
  - debug_events: 8 tests
  - win_condition: 15 tests
- `make cardgame` builds successfully
- `make preview`, `make biome_preview`, and `make card_enroll` build successfully
- `pkg-config --modversion raylib` reports `5.5.0`
- `Makefile` and `CMakeLists.txt` list the same main source groups by inspection
- CMake was not build-verified in this environment because `cmake` is not installed

---

## Implemented and Verified

| Feature | Files | Notes |
|---------|-------|-------|
| Database connection and card loading | `src/data/db.c`, `src/data/cards.c` | Loads cards and NFC UID mappings from SQLite |
| Card action registry and troop card dispatch | `src/logic/card_effects.c` | All current troop card types route through the play registry |
| Troop spawn pipeline | `src/logic/card_effects.c`, `src/entities/troop.c`, `src/systems/energy.c` | Enforces energy, reads JSON stats, spawns into canonical battlefield lanes |
| Base spawning and win latch | `src/entities/building.c`, `src/logic/win_condition.c`, `src/core/game.c` | Bases spawn at startup, lethal base hits latch `gameOver`/`winnerID`, and the match result overlay renders in-game |
| Entity lifecycle and state machine | `src/entities/entities.c` | Create, update, draw, destroy, idle/walk/attack/dead states |
| Canonical battlefield model | `src/core/battlefield.c`, `src/core/battlefield_math.c` | Shared world space, territory layout, spawn anchors, lane waypoints |
| Pathfinding and lane walking | `src/logic/pathfinding.c` | Troops walk canonical waypoint paths and idle at the end |
| Combat targeting and damage | `src/logic/combat.c` | Nearest/building targeting, range checks, hit-synced damage, base-kill latching |
| Split-screen viewport model | `src/rendering/viewport.c`, `src/systems/player.c` | Shared battlefield rendered from both player perspectives |
| Sprite animation loading/rendering | `src/rendering/sprite_renderer.c` | Base plus current troop sprite sets and all animation slots are loaded |
| Card visual rendering | `src/rendering/card_renderer.c` | Atlas-driven card frame/background/icon composition |
| Biome-aware tilemap rendering | `src/rendering/tilemap_renderer.c`, `src/rendering/biome.c` | Procedural territory tilemaps with biome definitions |
| Core game loop and NFC event handling | `src/core/game.c`, `src/hardware/nfc_reader.c`, `src/hardware/arduino_protocol.c` | Init, update, render, cleanup, keyboard test input, NFC polling |
| HUD basics | `src/rendering/ui.c` | Energy bars, viewport labels, and match-result overlays are implemented |
| Build/test maintenance | `Makefile`, `CMakeLists.txt`, `tests/*.c` | Main source lists are aligned; unit tests cover battlefield, math, pathfinding, combat, animation, debug events, and win conditions |

---

## Stubbed or Partial

| Feature | Files | Current state |
|---------|-------|---------------|
| Pregame / match flow | `src/systems/match.c`, `src/systems/match.h` | Header is empty; source contains declarations/comments only |
| Projectile system | `src/entities/projectile.c`, `src/entities/projectile.h` | Header is empty; source is comment-only |
| Spell gameplay effects | `src/logic/card_effects.c` | `play_spell()` spends energy and prints parsed data, but applies no gameplay effect |
| Base health and hand UI | `src/rendering/ui.c`, `src/rendering/card_renderer.c` | Match result text exists, but there are no base HP bars and cards are not rendered in the live match |
| Specific-target combat | `src/logic/combat.c`, `src/entities/troop.c` | `targetType` is parsed, but `TARGET_SPECIFIC_TYPE` still falls back to nearest-target behavior |
| Snow and swamp biome identity | `src/rendering/biome.c` | Both are still placeholders built from the grass tileset |
| Spawn system ownership | `src/systems/spawn.c` | File exists, but actual spawn logic still lives in `card_effects.c` and `troop.c` |
| Slot cooldown ownership | `src/systems/player.c`, `src/logic/card_effects.c` | Cooldowns tick down, but no play path sets them non-zero |

---

## Highest Priority

1. Add base-health UI.
   - Draw HP bars for both bases in `src/rendering/ui.c`.
   - Decide whether card/hand rendering should live in the same HUD pass.

2. Make spell cards do real gameplay work.
   - Replace the current debug-print path with actual targeting/effect resolution.

3. Build the projectile system if ranged attacks are part of the near-term plan.
   - Define the header API first.
   - Decide whether projectiles live in the battlefield entity list or in a dedicated pool.

4. Add a real pregame/match phase.
   - Replace the declarations in `match.c` with real state and transitions.
   - Gate gameplay start on player readiness / deck confirmation.

5. Normalize database reset behavior.
   - Decide whether `nfc_tags` should stay user-managed or get optional seed data.
   - Make `init-db` recreate a clean database shape when a full reset is intended.

---

## Secondary Cleanup

- Fix the macro redefinition warning in `src/rendering/biome.c` (`R` is defined twice).
- Move shared spawn orchestration out of `card_effects.c` if `src/systems/spawn.c` is meant to own it.
- Fill out the empty public headers for `match` and `projectile`.
- Decide whether `CardSlot.activeCard` should become real state or be removed.
