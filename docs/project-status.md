# Project Status

Verified from source and local commands on 2026-04-04.

## Project Summary

This repository is a local two-player C/Raylib card game with optional Arduino
NFC input, a canonical `Battlefield` world model, SQLite-backed card data,
stationary player bases, and three standalone tools (`card_preview`,
`biome_preview`, `card_enroll`).

The codebase is not a complete end-to-end match game yet. The current
implementation supports spawning troops, spawning one base per player, moving
units through canonical lane paths, finding targets, applying hit-synced combat
damage, playing death animations, latching match results on base destruction,
and rendering both players' views over the same battlefield. Pregame/match
phases, projectile gameplay, spell world effects, base-health UI, and in-game
card rendering are still missing.

## Verified Working Now

- `make cardgame` builds the main game successfully.
- `make preview`, `make biome_preview`, and `make card_enroll` all build successfully.
- `make test` passes all seven standalone test binaries:
  - `test_pathfinding`
  - `test_combat`
  - `test_battlefield_math`
  - `test_battlefield`
  - `test_animation`
  - `test_debug_events`
  - `test_win_condition`
- Runtime architecture is canonical:
  - one `1080 x 1920` battlefield
  - seam at `y = 960`
  - bottom and top territories rendered through separate player cameras
  - Player 2 rendered through `p2RT` and composited to the right viewport
- `game_init()` spawns one stationary base per player from `bf_base_anchor()`
  through `building_create_base()`.
- Base destruction latches `gameOver` and `winnerID`.
  - lethal hits from `combat_apply_hit()`, `combat_resolve()`, or
    `building_take_damage()` can trigger the authoritative latch
  - `win_check()` remains as a defensive fallback and supports a draw if both
    bases are already dead
- `game_render()` draws a rotated `VICTORY` / `DEFEAT` / `DRAW` overlay once
  the result is latched.
- Card data loads from SQLite through `db.c` and `cards.c`.
- NFC UID mappings load from the `nfc_tags` table at startup.
- Troop spawn flow is wired:
  - card lookup
  - energy check
  - JSON stat parse
  - canonical spawn position lookup
  - entity creation
  - Battlefield registry insert
- Troops path through canonical waypoints from `Battlefield`.
- Troops can acquire targets, play attack clips, apply damage at the attack hit
  marker, die, and get swept after the death clip finishes.
- Idle troops do reacquire nearby targets; they do not need an external state
  transition to re-enter combat.
- Debug rendering exists and is wired:
  - `F1` lane paths
  - `F2` attack bars
  - `F3` target lines
  - `F4` event flashes
  - `F5` attack range circles

## Partial Or Caveated Behavior

- The game starts directly in live play. There is no pregame/ready flow or
  game-over phase machine.
- Spell cards consume energy and print effect metadata, but they do not change
  game state.
- Card-type handlers for healer, assassin, brute, and farmer do not add custom
  behavior of their own.
  - Current gameplay differences come from per-card stats and targeting values
    parsed from card JSON.
  - `BRUTE_01` does already get building-priority targeting because its JSON
    sets `"targeting": "building"`.
- `TARGET_SPECIFIC_TYPE` is parsed from card JSON, but `combat_find_target()`
  currently falls back to nearest-target behavior because entities do not
  expose a type-name matcher.
- Card slots track `cooldownTimer`, but nothing currently sets a non-zero
  cooldown after play.
- `CardSlot.activeCard` exists on the struct but is not used by runtime
  gameplay code.
- `game_handle_test_input()` only plays `KNIGHT_01`.
- Both territories are still initialized as `BIOME_GRASS` in `game_init()`.
- There is no base HP UI and no in-game card hand display.

## Not Implemented

- `src/systems/match.c`
  - declarations only, no implementation
- `src/entities/projectile.c`
  - empty placeholder

Because of those gaps, the repo does not currently support:

- pregame readiness flow
- match restart/rematch flow
- projectile-driven units
- spell effects that alter world state
- visible base HP
- in-game card/hand rendering

## Database Reality

The checked-in runtime database and the seed SQL now agree on card IDs.

- Checked-in `cardgame.db`
  - 6 cards
  - all `card_id` values uppercase (`KNIGHT_01`, `ASSASSIN_01`, ...)
  - 2 NFC mappings in `nfc_tags`
- Fresh database from `sqlite/schema.sql` + `sqlite/seed.sql`
  - 6 cards
  - all `card_id` values uppercase (`KNIGHT_01`, `ASSASSIN_01`, ...)
  - 0 NFC mappings
- Re-seeded copy of the checked-in database
  - 6 cards
  - card rows stay aligned with the checked-in runtime IDs
  - still 2 NFC mappings

That remaining difference matters:

- the keyboard smoke path in `game.c` looks up `KNIGHT_01`
- it works with the checked-in `cardgame.db`
- it also works after recreating a fresh database from the current seed files
- running `init-db` against an existing checked-in `cardgame.db` preserves any
  existing `nfc_tags` rows because the init target does not clear the database
  first
- a true reset requires deleting `cardgame.db` first, then running the init
  target

## Verification Notes

- `raylib` was available locally through `pkg-config` as version `5.5.0`.
- `cmake` was not installed in this shell environment, so the CMake/CTest path
  was inspected from `CMakeLists.txt` but not executed here.
