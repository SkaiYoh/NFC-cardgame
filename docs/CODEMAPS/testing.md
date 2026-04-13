# Testing

Verified on 2026-04-13.

## What Exists

The repo uses standalone assert-based test executables, not an external C test
framework.

Current test binaries:

- `test_pathfinding`
- `test_combat`
- `test_entities`
- `test_battlefield_math`
- `test_battlefield`
- `test_animation`
- `test_debug_events`
- `test_spawn_fx`
- `test_spawn_placement`
- `test_deposit_slots`
- `test_assault_slots`
- `test_status_bars`
- `test_win_condition`
- `test_sustenance`
- `test_hand_ui`
- `test_card_effects`

## Local Result

`make test` compiled and passed all fifteen test executables successfully in this
environment.

## Coverage Areas

| Test Binary | Covers |
|------------|--------|
| `test_pathfinding` | canonical waypoint stepping, lane progression, movement direction, invalid lane handling |
| `test_combat` | canonical range checks, target selection, damage application, kill/clamp behavior, base-destruction latching |
| `test_entities` | attack-state transitions, healer stale-target cleanup, pursuit fallback behavior, and building attack clip lifecycle |
| `test_battlefield_math` | coordinate transforms, seam helpers, bounds, slot-to-lane mapping |
| `test_battlefield` | territory setup, spawn anchors, waypoint generation, entity registry, side mapping, base anchors |
| `test_animation` | clip playback, hit-marker behavior, animation policy, cycle calculations |
| `test_debug_events` | debug flash ring buffer behavior |
| `test_spawn_fx` | smoke FX atlas usage, frame stepping, expiry, and spawn registration |
| `test_spawn_placement` | non-overlapping spawn anchor search and failure cases |
| `test_deposit_slots` | base deposit slot reservation, promotion, and release behavior |
| `test_assault_slots` | base assault slot reservation, overflow queueing, and promotion |
| `test_status_bars` | troop/base status-bar frame selection and fallback rendering |
| `test_win_condition` | winner latching, fallback draw logic, null-safety, and base-destruction ownership checks |
| `test_sustenance` | sustenance placement, respawn, claim/release, and geometry exclusions |
| `test_hand_ui` | hand layout math, sparse compaction, animation frames, and shared-sheet row mapping |
| `test_card_effects` | card dispatch, King slot gating, no-spawn handling, and base attack trigger behavior |

## Testing Pattern

The tests intentionally compile production `.c` files directly with local
stubs.

Common pattern:

- predefine header guards to block heavy include chains
- define lightweight local stubs for `Vector2`, `Player`, `Battlefield`,
  Raylib types, or other dependencies
- include the production `.c` file directly
- call the real implementation under test

This pattern is used heavily in:

- `tests/test_pathfinding.c`
- `tests/test_combat.c`
- `tests/test_battlefield_math.c`
- `tests/test_battlefield.c`
- `tests/test_animation.c`
- `tests/test_win_condition.c`
- `tests/test_entities.c`
- `tests/test_card_effects.c`

## Build-System Exposure

- `Makefile`
  - builds and runs all fifteen tests with `make test`
- `CMakeLists.txt`
  - is currently stale relative to `Makefile` and does not register the full local test set

## What Is Not Covered By Automated Tests

- the full Raylib game loop in a real window
- hardware serial I/O against live Arduino devices
- SQLite data-layer behavior
- card renderer output correctness
- biome preview and card preview runtime behavior
- base spawning and match-result overlay inside the live Raylib window
- pregame/match-state flow
- projectile behavior

## CI Status

There is no CI workflow checked into the repository at the time of writing.

## Verification Caveat

`cmake` was not installed in this shell environment, so the CTest path was
inspected from `CMakeLists.txt` but not executed locally here. The `Makefile`
path was executed and passed.
