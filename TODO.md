# NFC Card Game — Project To-Do List

Infrastructure is solid — rendering, DB, cards, split-screen all work. What's missing is gameplay.

## Critical (No game without these)

| # | Feature | Files | Status |
|---|---------|-------|--------|
| 1 | **Entity system** — Troop/building/projectile spawn, update, and destroy logic | `src/entities/troop.c`, `building.c`, `projectile.c`, `entities.c` | Empty stubs |
| 2 | **Card effects → entity spawning** — Connect card play handlers to actually spawn entities on the board | `src/logic/card_effects.c` | Handlers exist but only `printf` debug output |
| 3 | **Combat system** — Targeting, damage calculation, attack resolution | `src/logic/combat.c` | Declaration only |
| 4 | **Pathfinding** — Troop movement toward opponents (A* or simple lane-based) | `src/logic/pathfinding.c` | Declaration only |
| 5 | **Win conditions** — Base health tracking, victory/defeat detection, end-game state | `src/logic/win_condition.c` | Declaration only |

## Important (Needed for balanced gameplay)

| # | Feature | Files | Status |
|---|---------|-------|--------|
| 6 | **Energy system** — Implement energy spending, capping, and regeneration (currently hardcoded in `player_init`) | `src/systems/energy.c` | Declaration only |
| 7 | **Spawn/placement system** — Card-to-entity placement, position validation, entity lifecycle | `src/systems/spawn.c` | Empty |
| 8 | **UI/HUD** — Energy bar, card hand display, opponent info, health bars | `src/rendering/ui.c` | Empty |
| 9 | **Makefile update** — Add entity, combat, pathfinding, win condition, energy, match, and spawn modules to `SOURCES` | `Makefile`, `CMakeLists.txt` | Missing from build |

## Nice to Have (Polish & full experience)

| # | Feature | Files | Status |
|---|---------|-------|--------|
| 10 | **Pregame/match lobby** — Player readiness, card selection, match setup phase | `src/systems/match.c` | Declaration only |
| 11 | **NFC hardware integration** — Arduino protocol, NFC reader communication, card scanning input | `src/hardware/nfc_reader.c`, `arduino_protocol.c` | Empty stubs |
| 12 | **Keyboard/mouse input fallback** — Allow playing without NFC hardware for testing | Not yet created | Missing |

## Suggested Implementation Order

1. **Entity system** (troop struct + spawn/update/destroy)
2. **Spawn system** (place entities on the board from card slots)
3. **Card effects** (wire up handlers to spawn system)
4. **Energy system** (enforce card costs)
5. **Pathfinding** (troops move toward enemy side)
6. **Combat** (troops attack when in range)
7. **Win conditions** (base health → game over)
8. **UI/HUD** (display energy, health, cards)
9. **Match/pregame** (setup phase)
10. **NFC hardware** (physical card scanning)

Roughly **800–1200 lines** of gameplay code separate the project from a playable prototype.
