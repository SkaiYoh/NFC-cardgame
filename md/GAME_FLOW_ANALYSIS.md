# NFC Cardgame — Complete Game Flow Analysis

> Generated: 2026-02-20
> Branch: `main` (clean, commit `4d9d01e`)

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Game Flow Diagram](#2-game-flow-diagram)
3. [Initialization Flow](#3-initialization-flow)
4. [Main Loop (Update → Render)](#4-main-loop-update--render)
5. [Card Play Pipeline](#5-card-play-pipeline)
6. [Entity Lifecycle](#6-entity-lifecycle)
7. [Rendering Pipeline](#7-rendering-pipeline)
8. [Module-by-Module Analysis](#8-module-by-module-analysis)
9. [Stub / Unimplemented Systems](#9-stub--unimplemented-systems)
10. [Cross-Cutting Issues & Suggestions](#10-cross-cutting-issues--suggestions)

---

## 1. Architecture Overview

```
┌──────────────────────────────────────────────────────────────────┐
│                         GameState (types.h)                      │
│                                                                  │
│  db: DB          deck: Deck         cardAtlas: CardAtlas         │
│  players[2]      biomeDefs[4]       spriteAtlas: SpriteAtlas     │
│  halfWidth       currentPlayerIndex                              │
└──────────────────────────────────────────────────────────────────┘
         │                  │                    │
         ▼                  ▼                    ▼
   ┌──────────┐     ┌──────────────┐    ┌──────────────────┐
   │  DB layer│     │  Card layer  │    │  Rendering layer │
   │  db.c    │     │  cards.c     │    │  biome.c         │
   │  libpq   │     │  card_       │    │  tilemap_        │
   │          │     │  effects.c   │    │  renderer.c      │
   └──────────┘     └──────────────┘    │  card_renderer.c │
                                        │  sprite_         │
                                        │  renderer.c      │
                                        │  viewport.c      │
                                        └──────────────────┘
         │
         ▼
   ┌──────────────────────────────────────────┐
   │  Player (types.h)                        │
   │  playArea, screenArea, camera            │
   │  tilemap, biomeDef, tileDefs             │
   │  slots[3] (CardSlots / NFC readers)      │
   │  entities[] (troops, buildings)          │
   │  energy, maxEnergy, energyRegenRate      │
   └──────────────────────────────────────────┘
         │
         ▼
   ┌──────────────────────────────────────────┐
   │  Entity (types.h)                        │
   │  id, type, faction, state                │
   │  position, moveSpeed                     │
   │  hp, attack, attackSpeed, attackRange    │
   │  anim (AnimState), sprite, spriteScale   │
   │  ownerID, lane, alive, markedForRemoval  │
   └──────────────────────────────────────────┘
```

**Key Structural Decisions:**

- `GameState` is a single monolithic struct owned by `main()`. All subsystems receive pointers to it.
- `Player` embeds its `TileMap` and biome tile definitions by value — no separate heap allocation for those arrays.
- `Entity` is always heap-allocated via `entity_create()` and referenced by pointer in `Player.entities[]`.
- The `currentPlayerIndex` field on `GameState` is a global side-channel used to pass the "acting player" into card effect callbacks. This is a design smell (see §10).

---

## 2. Game Flow Diagram

```
main()
  │
  ├─► game_init(GameState*)
  │     ├─► db_init()            — connect to PostgreSQL via env DB_CONNECTION
  │     ├─► cards_load()         — SELECT * FROM cards → heap-allocated Deck
  │     ├─► InitWindow()         — Raylib window 1920×1080 @ 60 FPS
  │     ├─► card_action_init()   — register type→handler map (static array)
  │     ├─► card_atlas_init()    — load card sprite sheet, build rect lookup
  │     │     (TEST) card_action_play(each card, NULL) ← plays all cards with NULL state
  │     ├─► biome_init_all()     — load all 4 biome textures, compile TileDef arrays
  │     ├─► sprite_atlas_init()  — load all character animation sheets (6 types × 6 anims)
  │     └─► viewport_init_split_screen()
  │             ├─► player_init(P1, BIOME_GRASS, rot=+90°, seed=42)
  │             │     ├─► biome_copy_tiledefs()
  │             │     ├─► tilemap_create_biome()  — procedural tile grid (weighted random)
  │             │     └─► player_init_card_slots() — 3 NFC slot world positions
  │             └─► player_init(P2, BIOME_UNDEAD, rot=−90°, seed=99)
  │
  └─► while(!WindowShouldClose())
        │
        ├─► game_update(GameState*)
        │     ├─► game_handle_test_input()   — KEY_1 / KEY_Q test card plays
        │     ├─► player_update(P1, dt)
        │     │     ├─► energy regen (capped at maxEnergy)
        │     │     └─► slot cooldown countdown
        │     ├─► player_update(P2, dt)
        │     ├─► player_update_entities(P1, gs, dt)
        │     │     ├─► entity_update() × entityCount
        │     │     │     └─► WALKING: move Y−=speed×dt, check despawn, flip dir
        │     │     └─► sweep markedForRemoval (swap-with-last, entity_destroy)
        │     └─► player_update_entities(P2, gs, dt)
        │
        └─► game_render(GameState*)
              ├─► BeginDrawing() / ClearBackground(RAYWHITE)
              │
              ├─► [P1 viewport]
              │     ├─► viewport_begin(P1) — BeginScissorMode + BeginMode2D
              │     ├─► viewport_draw_tilemap(P1)
              │     │     ├─► tilemap_draw()         — base tiles
              │     │     ├─► tilemap_draw_details()  — detail overlay
              │     │     └─► tilemap_draw_biome_layers() — extra PAINT/RANDOM layers
              │     ├─► game_draw_entities_for_viewport(P1)
              │     │     — both players' entities drawn: owners normally,
              │     │       crossed entities mirrored into opponent viewport
              │     ├─► DrawText("PLAYER 1", ...)
              │     └─► viewport_end() — EndMode2D + EndScissorMode
              │
              ├─► [P2 viewport] (same structure)
              │
              └─► EndDrawing()
```

---

## 3. Initialization Flow

### 3.1 `game_init` — `src/core/game.c:15`

| Step | Call | Notes / Issues |
|------|------|----------------|
| 1 | `db_init(&g->db, conninfo)` | `conninfo` sourced from `getenv("DB_CONNECTION")`. If env var is unset, `conninfo == NULL` → `db_init` returns false. **No fallback / helpful error message to user.** |
| 2 | `cards_load(&g->deck, &g->db)` | Heap-allocates `Card[]`. On failure, calls `db_close` then returns false. |
| 3 | `InitWindow(1920, 1080, ...)` | Called **after** DB init — window exists only if DB succeeds. Console errors go unnoticed without a terminal. |
| 4 | `card_action_init()` | Registers 6 handlers into a static global array. Idempotent only if `handler_count` is reset (it is). |
| 5 | `card_atlas_init()` | Loads `CARD_SHEET_PATH`. Texture load after `InitWindow` is correct. |
| **TEST** | `card_action_play(card, NULL)` × deck | **Bug**: calls `play_*` with `state == NULL`. `spawn_troop_from_card` checks `!state` and returns early (safe), but this silently exercises the entire effect dispatch on startup with no real purpose beyond debugging. Should be removed or gated behind a `#ifdef DEBUG`. |
| 6 | `biome_init_all(g->biomeDefs)` | Loads 4 biomes; `BIOME_UNDEAD` uses `biome_define_undead2()` — the function named `biome_define_undead()` exists but is NOT called (dead code). |
| 7 | `sprite_atlas_init()` | Loads 5 character types × 6 animations = 30 textures. No error checking on individual `LoadTexture` calls. |
| 8 | `viewport_init_split_screen()` | Calls `player_init` for each player with hardcoded seeds 42 and 99. |

### 3.2 `player_init` — `src/systems/player.c:17`

| Step | Call | Notes / Issues |
|------|------|----------------|
| 1 | `biome_copy_tiledefs()` | `memcpy` of TILE_COUNT (32) TileDefs — copies pointer-to-texture, which still points into `gs->biomeDefs`. **Dangling pointer risk** if biomeDefs are moved or freed before players. |
| 2 | `tilemap_create_biome()` | Calls `srand(seed)` — **global PRNG state**. Both calls happen sequentially so seeds don't interfere with each other, but any code calling `rand()` between player inits will shift results. |
| 3 | `player_init_card_slots()` | Spawn Y is `playArea.y + playArea.height * 0.8f` for both players. Since playAreas differ in origin, this is correct. **But `lane` field on Entity is never set** — it's always 0. |
| 4 | Energy defaults | `energy=5.0, max=10.0, regen=1.0` — hardcoded; `energy.c` has stubs for proper management. |

---

## 4. Main Loop (Update → Render)

### 4.1 `game_update` — `src/core/game.c:71`

#### `player_update` — `src/systems/player.c:107`
- Energy regen is a simple `energy += rate * dt`, clamped to `maxEnergy`.
  **Issue**: No `energy_consume()` called on card play — energy is never actually spent. Cards are free.
- Slot cooldown counts down but `isOccupied` is never set to `true` anywhere in the current code. The slot system is cosmetic only.

#### `player_update_entities` — `src/systems/player.c:83`
- Updates then sweeps dead entities via swap-with-last.
  **Issue**: Swap-with-last changes iteration order mid-loop, but since it's a separate backwards sweep loop after the update loop, this is safe.
- `entity_destroy(dead)` is `free(e)`. No sprite/resource cleanup — Entity holds only a **pointer** to `CharacterSprite`, not ownership, so this is intentional and correct.

#### `entity_update` — `src/entities/entities.c:57`
- `ESTATE_WALKING`: Moves entity Y downward (`position.y -= speed * dt`).
  - **Despawn condition**: `position.y < owner->playArea.y - owner->playArea.height * 0.9f`
    This can be negative for large play areas. The geometry is: playArea.y=0, height≈540, so despawnY = 0 − 486 = −486. Entity must travel far below screen to despawn. This seems intentional (crossing opponent's entire area) but the threshold is very aggressive — entity can travel far off-screen before removal.
  - **Direction flip**: `e->anim.dir = (position.y > borderY) ? DIR_UP : DIR_DOWN`
    `borderY = owner->playArea.y` (= 0 for both players). Since playArea starts at y=0, entities spawn at y≈432 (0.8 × 540) and walk upward toward y=0. They start with `dir=DIR_UP` from `entity_create()` → correct.
    **Issue**: When entity crosses into opponent space (y < 0), direction flips to `DIR_DOWN`. In the viewport draw code, the opponent's viewport mirrors the entity — this could cause double-flip artifacts depending on camera rotation.
- `ESTATE_DEAD`: Immediately marks for removal. No death animation plays out — `ANIM_DEATH` is set by `entity_set_state` but entity is removed next frame before any animation frames render.
- `ESTATE_IDLE`: No behavior — entity sits in place. No combat logic triggers state changes from WALKING→IDLE or IDLE→ATTACK.

### 4.2 `game_render` — `src/core/game.c:116`

- Rendering is split into two scissored viewports.
- `game_draw_entities_for_viewport` iterates over **all entities from both players** for each viewport.
  **Performance note**: O(2 × totalEntities) draw calls per frame (each entity may be drawn twice). At MAX_ENTITIES=64 per player, this is 128 entities × 2 passes = 256 potential draw iterations. Acceptable at this scale.
- **Crossed-entity mirror math** (`game.c:98–111`): When an entity's `position.y < owner->playArea.y` (i.e., has entered opponent's half), it's mirrored into the opponent's viewport. The lateral flip `1.0f - lateral` is correct. `depth = owner->playArea.y - e->position.y` grows as entity moves further into opponent territory.
  **Issue**: The `AnimState` is stack-copied (`AnimState crossed = e->anim`) and direction forced to `DIR_DOWN`. This is a quick workaround but the entity also plays whatever animation it happens to be in — not necessarily a correct "walking toward base" animation.
  **Issue**: `entity_draw` is called on the original entity in owner's viewport even when `position.y < 0` — the camera's scissor mode clips it, but it's still submitted to the GPU. Minor inefficiency.

---

## 5. Card Play Pipeline

```
[Input: KEY_1 or KEY_Q] (test only — NFC not integrated)
        │
        ▼
game_test_play_knight(gs, playerIndex)
        │
        ├─► cards_find(&gs->deck, "KNIGHT_001")   — O(n) linear scan
        │
        ├─► gs->currentPlayerIndex = playerIndex   ← GLOBAL SIDE-CHANNEL
        │
        └─► card_action_play(card, gs)
                │
                └─► handlers[] dispatch by card->type  (strcmp O(n) over 6 handlers)
                        │
                        └─► play_knight(card, gs)
                                │
                                └─► spawn_troop_from_card(card, gs)
                                        │
                                        ├─► player_center(player)  ← default spawn
                                        │
                                        ├─► [for each slot] player_slot_is_available()
                                        │     — uses first available slot's spawn pos
                                        │
                                        ├─► troop_create_data_from_card(card)
                                        │     └─► cJSON_Parse(card->data)
                                        │           — parses hp, attack, speed, etc.
                                        │           — cJSON_Delete(root) ← cleans up
                                        │
                                        └─► troop_spawn(player, &data, spawnPos, atlas)
                                                │
                                                ├─► entity_create(ENTITY_TROOP, faction, pos)
                                                │     — malloc + memset
                                                │     — anim_state_init(ANIM_IDLE, DIR_UP)
                                                │
                                                ├─► Copy stats from TroopData → Entity
                                                ├─► e->sprite = sprite_atlas_get(atlas, type)
                                                │
                                                └─► entity_set_state(e, ESTATE_WALKING)
                                                      — resets anim to ANIM_WALK
                                                      — player_add_entity(player, e)
```

### Card Play Issues

| Issue | Location | Severity |
|-------|----------|----------|
| `currentPlayerIndex` is a global side-channel on GameState | `game.c:59`, `card_effects.c:55` | Medium — not thread-safe, fragile API |
| `card_action_play()` called with `NULL` state during init test | `game.c:35` | Low — guarded by `!state` check, but noise |
| Energy cost (`card->cost`) never checked or deducted | `card_effects.c:48–72` | High — core game mechanic missing |
| `cards_find()` is O(n) linear scan with `strcmp` | `cards.c:53` | Low — deck will be small (< 100 cards) |
| `card_action_register()` limit is hardcoded `MAX_HANDLERS=32` | `card_effects.c:14` | Low — adequate for current scope |
| `play_spell()` only prints to console — no game effect | `card_effects.c:74` | Medium — spell cards do nothing in-game |
| `data.targetType` stores pointer into cJSON tree, then `cJSON_Delete` | `troop.c:73–76` | **High** — dangling pointer; `targetType` in TroopData becomes invalid immediately after `cJSON_Delete(root)`. If anything reads `data.targetType` after `troop_create_data_from_card` returns, it's UB. |
| Slot `isOccupied` never set to true after spawn | `player_init_card_slots`, `spawn_troop_from_card` | Medium — slot system doesn't gate replays |

---

## 6. Entity Lifecycle

```
entity_create()       ← malloc, ESTATE_IDLE, ANIM_IDLE
      │
entity_set_state(WALKING)   ← ANIM_WALK, dir=DIR_UP
      │
player_add_entity()   ← appended to Player.entities[]
      │
      ▼  [per frame]
entity_update()
  WALKING:
    position.y -= moveSpeed * dt
    if position.y < despawnY → markedForRemoval = true
    anim.dir = DIR_UP / DIR_DOWN based on border crossing
  DEAD:
    markedForRemoval = true  (instant, no death anim)
      │
      ▼  [end of frame]
player_update_entities():
  sweep backwards, swap-with-last, entity_destroy()
      │
entity_destroy()      ← free(e)  (sprite pointer NOT freed — correct, atlas owns it)
```

### Entity Issues

| Issue | Location | Severity |
|-------|----------|----------|
| `ESTATE_DEAD` marks removal immediately — no death animation plays | `entities.c:83` | Medium — visual quality |
| `entity.lane` is set to 0 always; never populated by spawner | `troop.c:80–107` | Medium — multi-lane targeting can't work |
| No combat system wired to entity updates | `combat.c` stubs | **High** — entities walk through each other |
| No targeting — entities ignore all other entities | `entity_update()` | **High** — core game mechanic missing |
| `s_nextEntityID` is a static global — resets on restart (non-issue in practice) but never rolls over | `entities.c:10` | Low |
| `entity_create` sets `spriteScale = 2.0f` hardcoded | `entities.c:24` | Low — overridden by troop_spawn, but fragile |
| `Player.base` is always NULL — `building_create_base()` returns NULL | `building.c:7` | High — win condition can't work |

---

## 7. Rendering Pipeline

### 7.1 Tilemap Rendering

```
viewport_draw_tilemap(Player*)
  ├─► tilemap_draw()              — base layer: cells[] → TileDef → DrawTexturePro
  ├─► tilemap_draw_details()      — detail overlay: detailCells[] (-1 = skip)
  └─► tilemap_draw_biome_layers() — biome layers (RANDOM or PAINT)
        ├─► RANDOM: biomeLayerCells[li][] allocated in tilemap_create_biome
        └─► PAINT: static const int cells[][3] compiled into biome.c
```

#### Tilemap Issues

| Issue | Location | Severity |
|-------|----------|----------|
| `tilemap_draw()` indexes `tileDefs[cells[...]]` without bounds check | `tilemap_renderer.c:158` | **High** — out-of-bounds if cell value ≥ TILE_COUNT (32). Random generation can produce values up to `blockStart[n] + blockSize[n] - 1`; if any block puts tile index ≥ 32, this is a buffer overread. |
| `tilemap_create` (non-biome) uses hardcoded `(rand()%10<8) ? rand()%16 : 16+rand()%16` — always produces indices 0–31, safe, but `tilemap_create` is now unused (all code uses `tilemap_create_biome`) | `tilemap_renderer.c:30` | Low — dead code |
| `srand(seed)` called inside `tilemap_create_biome` — contaminates global PRNG after call | `tilemap_renderer.c:73` | Medium — any subsequent `rand()` calls will be in an unknown-seeded state |
| `tilemap_draw_biome_layers` passes `BiomeDef*` through a forward-declared struct pointer; `biome.h` includes `tilemap_renderer.h` but `tilemap_renderer.h` forward-declares `BiomeDef` — circular-ish dependency managed by order | `tilemap_renderer.h:65` | Low — works but fragile |
| `tilemap_draw_details` detail tiles can be larger than `tileSize` (e.g., 40×21 px detail on 32px tiles) causing overlap into adjacent tiles | `tilemap_renderer.c:181–188` | Low — visual only |

### 7.2 Sprite Rendering

```
sprite_draw(CharacterSprite*, AnimState*, Vector2 pos, float scale)
  ├─► sheet = cs->anims[state->anim]
  ├─► col = state->frame % sheet->frameCount   ← modulo bounds frame
  ├─► row = state->dir                          ← DIR_DOWN/SIDE/UP = 0/1/2
  └─► DrawTexturePro(src, dst, origin={dw/2,dh/2}, 0°, WHITE)
        — sprite centered on position
```

#### Sprite Issues

| Issue | Location | Severity |
|-------|----------|----------|
| `load_sheet` computes `frameHeight = texture.height / DIR_COUNT` — assumes exactly 3 rows (DOWN, SIDE, UP). If a sheet has a different layout, this silently gives wrong frame height | `sprite_renderer.c:16` | Medium |
| If `LoadTexture` fails (file not found), `texture.id == 0`; `sprite_draw` checks `sheet->texture.id == 0` and returns — safe, but no error is logged | `sprite_renderer.c:98` | Medium |
| `ANIM_RUN` is defined and loaded but no code ever sets `ANIM_RUN` state on entities | `sprite_renderer.h:13` | Low — unused asset load |
| `anim_state_update` never resets `frame` to 0 — frame counter grows unboundedly; wrapping via `% frameCount` in draw is correct, but `state->frame` can overflow `int` after ~2.1 billion increments at 60fps (~1 year runtime) | `sprite_renderer.c:140–148` | Very Low |
| `sprite_type_from_card` returns `SPRITE_TYPE_KNIGHT` as default for unknown/NULL type — silently uses wrong sprite for unrecognized card types | `sprite_renderer.c:158` | Low |

### 7.3 Card Renderer

- `card_atlas_init()` loads one texture sheet and builds all `Rectangle` lookup arrays.
- `card_draw_ex()` renders up to 11 layered sprites per card call.
- `card_visual_from_json()` handles both bare JSON objects and wrapped strings — **defensive but complex**; the wrapped-string fallback suggests inconsistent DB data formats.
- `card_draw` / `card_draw_back` are defined but **never called from game.c** — cards are not rendered in the main game viewport. Only used in `tools/card_preview.c`.

#### Card Renderer Issues

| Issue | Location | Severity |
|-------|----------|----------|
| `card_atlas_init` has no error check on `LoadTexture` | `card_renderer.c:218` | Medium |
| `atlas->energy_full[]` array is declared in `CardAtlas` struct but never populated in `init_rects()` | `card_renderer.h:88` | Low — unused field |
| `card_visual_from_json` wraps malformed JSON in `{...}` and retries — this is a workaround for DB data inconsistency, not a documented format | `card_renderer.c:376–385` | Low — tech debt |
| Cards are never actually drawn to the game screen | — | **High** — no in-game card hand/slot UI |

### 7.4 Viewport / Split-Screen

```
P1 playArea:  { x=0,    y=0, w=1080, h=960 }   (world space, rotated 90°)
P1 screenArea:{ x=0,    y=0, w=960,  h=1080 }  (screen space, left half)
P1 camera:    target=center(playArea), rotation=+90°

P2 playArea:  { x=960,  y=0, w=1080, h=960 }
P2 screenArea:{ x=960,  y=0, w=960,  h=1080 }
P2 camera:    target=center(playArea), rotation=−90°
```

#### Viewport Issues

| Issue | Location | Severity |
|-------|----------|----------|
| P1 `playArea.x=0, width=1080` but P2 `playArea.x=960, width=1080` — they overlap by 120px in world X space. Entities in overlap zone would appear in both viewports before scissoring. This may be intentional (shared center strip) but is undocumented | `viewport.c:13–38` | Medium |
| `viewport_begin` calls `BeginScissorMode` then `BeginMode2D` — **order matters** for Raylib. This is correct (scissor applies to what's drawn in 2D mode). But `EndMode2D` then `EndScissorMode` in `viewport_end` is also correct. | `viewport.c:51–64` | OK |
| `viewport_draw_card_slots_debug` is commented out in `game_render` | `game.c:128,139` | Low — debug aid |
| No divider line between P1 and P2 viewports | — | Low — visual |

---

## 8. Module-by-Module Analysis

### 8.1 `src/data/db.c`

**Functions:**
- `db_init(DB*, const char*)` — connects via `PQconnectdb`. Properly sets `connected=false` and `conn=NULL` on failure after `PQfinish`.
- `db_query(DB*, const char*)` — executes raw SQL. **No parameterization** — SQL injection risk if queries ever include user-derived data. Currently only hardcoded queries are used, so risk is low.
- `db_query_params(DB*, query, nparams, params)` — parameterized form exists and should always be preferred for user data.
- `db_error(DB*)` — returns `PQerrorMessage`. Caller must not store the pointer (libpq invalidates it on next call).

**Issues:**
- `db_query` returns `NULL` on failure but also calls `PQclear(res)` before returning — callers never need to free on NULL return. This is correct but should be documented.
- No connection retry or reconnect logic — a dropped Postgres connection kills the game silently.
- `db_close` guards on `db->connected` but not on `db->conn != NULL` separately. If somehow `connected=true` with `conn=NULL` (shouldn't happen), `PQfinish(NULL)` is undefined behavior.

### 8.2 `src/data/cards.c`

**Functions:**
- `cards_load(Deck*, DB*)` — `calloc(rows, sizeof(Card))` + `strdup` per field. If any `strdup` returns NULL (OOM), the allocated card has NULL fields but the function doesn't detect this per-field and continues silently.
- `cards_free(Deck*)` — correctly frees all `strdup`'d strings. `rules_text` can be NULL (checked on load), but `free(NULL)` is safe in C.
- `cards_find(Deck*, const char*)` — O(n) linear search. Deck expected to be small.

**Issues:**
- `atoi(PQgetvalue(res, i, 2))` for `cost` — `atoi` returns 0 on failure (not distinguishable from a legitimate 0-cost card). Use `strtol` with error checking instead.
- No validation that `card_id`, `name`, `type` are non-empty before use.
- `c->data = strdup(PQgetvalue(res, i, 5))` — `data` column is `data::text` cast from JSONB. If the column is NULL, `PQgetisnull` is not checked here (unlike `rules_text`), so `strdup` would be called on an empty string `""` — safe but inconsistent with how `rules_text` is handled.

### 8.3 `src/logic/card_effects.c`

**Functions:**
- `card_action_init()` — clears handler count, registers 6 handlers.
- `card_action_register()` — appends to static `handlers[MAX_HANDLERS]`. No duplicate-type checking.
- `card_action_play()` — O(n) `strcmp` dispatch. Returns `false` if no handler found (logs to stdout).
- `spawn_troop_from_card()` — extracts player from `state->currentPlayerIndex`, finds slot, creates troop. **The currentPlayerIndex side-channel is the main design concern.**

**Issues:**
- `play_healer`, `play_assassin`, `play_brute`, `play_farmer` all call `spawn_troop_from_card` identically — no type-specific behavior. Healer has no healing logic, assassin has no stealth, etc.
- `play_spell` only `printf`s — no actual game effect.
- `handlers[i].type` stores a **string literal pointer** — safe as long as `card_action_register` is called with literal strings. If called with a stack buffer, this would be a dangling pointer.

### 8.4 `src/systems/player.c`

**Functions:** All documented in `player.h`. Well-structured.

**Issues:**
- `player_update_entities` and `player_draw_entities` both exist, but `player_draw_entities` is **never called** — drawing is done in `game_draw_entities_for_viewport` instead. Dead function.
- `player_remove_entity` removes by ID and calls `entity_destroy`. But `player_update_entities` also destroys removed entities. If both are called for the same entity ID, double-free occurs. No caller currently calls both, but the API is dangerous.
- `player_lane_pos()` is defined and never called anywhere in the main game. Useful for future lane-based spawning.
- Energy fields are initialized in `player_init` but `energy_init()` in `energy.c` is declared and stubbed — two initialization paths exist, inconsistently.

### 8.5 `src/entities/entities.c`

**Functions:**
- `entity_create` — malloc + memset + id assignment from static counter.
- `entity_destroy` — bare `free(e)`.
- `entity_set_state` — transitions with animation reset. `(void)oldState` suppresses unused-var warning — the old state is unused, so no transition-from logic exists.
- `entity_update` — only WALKING behavior; IDLE and DEAD are minimal stubs.

**Issues:**
- No combat logic in `entity_update`. Entities walk and despawn; they never fight.
- `entity_set_state` suppresses `oldState` — this means there's no "state machine" validation (e.g., DEAD → WALKING transition would be allowed).
- Frame animation in `anim_state_update` ticks unconditionally — dead entities continue animating until removed.

### 8.6 `src/rendering/biome.c`

**Two UNDEAD definitions:**
- `biome_define_undead()` (line 33) — simple definition, **never called**. Dead code.
- `biome_define_undead2()` (line 88) — generated by `biome_preview` tool, has 5 paint/random layers. This is what's actually used.

**Issues:**
- `biome_define_undead()` is dead code — confusing naming. Should be removed or renamed.
- `biome_init_all` calls `biome_define_undead2` for BIOME_UNDEAD — the name leaks implementation details.
- `biome_compile_blocks` silently truncates at TILE_COUNT (32) tiles — if a biome defines more than 32 base tiles, extras are dropped without warning.
- `biome_free_all` does not reset `biomeLayerCells` pointers on TileMap — but TileMap freeing is done by `tilemap_free`, not here. Correct, but the ownership split (BiomeDef owns textures; TileMap owns cell arrays) should be documented.
- `biome_fill_def` exposes `biome_define_undead2` via the public API under the `BIOME_UNDEAD` enum. The "2" naming convention should not be in the public interface.

### 8.7 `src/rendering/sprite_renderer.c`

- `sprite_atlas_init()` loads textures after `InitWindow()` — correct.
- No null/failure check on `LoadTexture` returns. If a texture file is missing, `texture.id == 0`; the `sprite_draw` guard handles draw-time failure but init continues silently.
- **Brute** uses `block.png` as attack animation (frame count 4) — likely "block" animation reused as attack. Semantically odd.
- All character paths are relative (e.g., `src/assets/characters/Knight/idle.png`) — working directory must be project root. No path validation.

### 8.8 `src/hardware/` (Stubs)

- `nfc_reader.c` — only `#include "nfc_reader.h"`. Completely empty.
- `arduino_protocol.c` — only `#include "arduino_protocol.h"`. Completely empty.
- `NFCEvent` struct defined but never used in the main game loop.
- `match.c` declares `pregame_init`, `pregame_update`, etc. but none are implemented or called.

**This is the largest gap between the physical game concept and the current implementation.** The game currently uses keyboard shortcuts (`KEY_1`, `KEY_Q`) as a test harness.

---

## 9. Stub / Unimplemented Systems

| System | Files | Status | Priority |
|--------|-------|--------|----------|
| NFC hardware integration | `nfc_reader.c`, `arduino_protocol.c` | Empty stub | **Critical** (core game mechanic) |
| Combat / damage | `combat.c` | Stub (fn declarations only) | **High** |
| Win condition | `win_condition.c` | Stub | **High** |
| Energy costs on card play | `player.c`, `card_effects.c` | Not wired | **High** |
| Pathfinding | `pathfinding.c` | Stub | **High** |
| Building entities (base) | `building.c` | Returns NULL | **High** |
| Projectiles | `projectile.c` | Empty stub | Medium |
| Pregame / lobby | `match.c` | Stub | Medium |
| Energy subsystem | `energy.c` | Stub (declarations only in .c, not .h body) | Medium |
| Spawn system | `spawn.c` | Empty | Medium |
| UI | `ui.c` | Empty | Medium |
| Lane-based targeting | `entity.lane` | Field exists, never set | Medium |
| Card slot occupancy | `CardSlot.isOccupied` | Never set true | Medium |
| Card slot cooldowns | `CardSlot.cooldownTimer` | Counts down, never set | Low |

---

## 10. Cross-Cutting Issues & Suggestions

### 10.1 `currentPlayerIndex` Side-Channel (Design Issue)

**Problem:** `GameState.currentPlayerIndex` is set in `game_test_play_knight` before calling `card_action_play`. The card effect callback reads it via `state->currentPlayerIndex`. This is an implicit parameter passed through a global struct field.

**Risk:** If card effects are ever triggered asynchronously (e.g., via NFC events from interrupts or a future event queue), this is a race condition. It also makes the API of `CardPlayFn` misleading — the actual player is not in the function signature.

**Suggestion:** Add `int playerIndex` directly to `CardPlayFn` signature: `typedef void (*CardPlayFn)(const Card*, GameState*, int playerIndex)`.

---

### 10.2 Missing Error Handling on Texture Loads

Every `LoadTexture` call in `sprite_atlas_init`, `card_atlas_init`, and `biome_init_all` can return a texture with `id == 0` on failure (file missing). None of these functions check for failure or log a specific error.

**Suggestion:** After each `LoadTexture`, check `texture.id == 0` and log the path that failed. Consider returning `false` from `sprite_atlas_init` so `game_init` can fail gracefully.

---

### 10.3 `data.targetType` Dangling Pointer (Bug)

In `troop_create_data_from_card` (`troop.c:73–76`):
```c
cJSON *tgtType = cJSON_GetObjectItem(root, "targetType");
if (tgtType && cJSON_IsString(tgtType)) {
    data.targetType = tgtType->valuestring;  // ← points into cJSON tree
}
cJSON_Delete(root);  // ← frees the tree, data.targetType now dangling
return data;
```
`TroopData.targetType` is `const char*`. After `cJSON_Delete(root)`, this pointer is invalid. Since `TroopData` is currently unused beyond troop stat initialization, the pointer is never read — but it's latent UB waiting to cause issues when targeting is implemented.

**Fix:** `data.targetType = strdup(tgtType->valuestring)` and ensure callers free it (or make `targetType` a fixed char buffer).

---

### 10.4 `srand` Global PRNG Contamination

`srand(seed)` is called in `tilemap_create_biome` and implicitly expected to produce deterministic tilemaps per seed. However:
- Any `rand()` call between `player_init(P1)` and `player_init(P2)` changes P2's map.
- Future systems (card shuffle, AI randomness) sharing the same global PRNG will affect determinism.

**Suggestion:** Use a separate per-player PRNG state (e.g., a simple LCG struct) instead of the global `rand()`.

---

### 10.5 Memory Management — `strdup` Failures Undetected

`cards_load` calls `strdup` for each card field. If `strdup` returns NULL (OOM):
- The `Card` struct has a NULL field.
- The loop continues, leaving a partially-initialized deck.
- `cards_free` will call `free(NULL)` which is safe but data is corrupted.

**Suggestion:** Check each `strdup` return; on failure, partially free and return false.

---

### 10.6 No Game State Machine

There is no `GamePhase` enum or state machine (pregame → playing → postgame). The game jumps straight into the play loop. `match.c` has stubs for `pregame_*` but they are not called from `main`.

**Suggestion:** Add a `GamePhase` to `GameState` and dispatch update/render through it. This is foundational for NFC reader integration (scan cards during pregame to build hand, then play during match).

---

### 10.7 Double-Initialization Risk in Biome Init

`biome_init_all` calls `memset(biomeDefs, 0, ...)` then calls `biome_define_undead2` which assigns `static const int cells[][3]` arrays to `BiomeLayer.cells`. If `biome_init_all` were called twice (not currently possible, but defensive concern), the second `memset` would zero all pointers including `cells`, then re-assign them — OK. The static arrays themselves are fine since they're in BSS/rodata.

---

### 10.8 Hardcoded Seeds and Player Assignment

- Player 1 always gets `BIOME_GRASS`, Player 2 always gets `BIOME_UNDEAD`.
- Seeds are `42` and `99` — no randomization or save/load for map reproducibility.
- **Suggestion:** Randomize seed or derive from a match ID. Allow biome selection.

---

### 10.9 No Delta-Time Cap

`GetFrameTime()` can return large values on frame stalls (debug breakpoints, OS preemption). Entity movement `position.y -= speed * dt` can teleport entities if `dt` is large.

**Suggestion:** Cap `deltaTime = fminf(dt, 1.0f/20.0f)` to prevent large jumps.

---

### 10.10 `energy_full` Array Never Populated

`CardAtlas.energy_full[CLR_COUNT]` is declared in `card_renderer.h:88` but never populated in `init_rects()`. This wastes struct memory and confuses future readers.

---

## Summary Priority Table

| Priority | Issue | File |
|----------|-------|------|
| **Critical** | NFC hardware completely unimplemented | `nfc_reader.c`, `arduino_protocol.c` |
| **High** | No combat/damage system — entities walk through each other | `combat.c` (stub) |
| **High** | Energy cost never deducted on card play | `card_effects.c`, `player.c` |
| **High** | `building_create_base()` returns NULL — no base, no win condition | `building.c` |
| **High** | `data.targetType` dangling pointer after `cJSON_Delete` | `troop.c:73–76` |
| **High** | No game phase / state machine — game boots directly into play | `game.c`, `match.c` |
| **Medium** | `currentPlayerIndex` side-channel in GameState | `game.c`, `card_effects.c` |
| **Medium** | `tilemap_draw` has no bounds check on tile index | `tilemap_renderer.c:158` |
| **Medium** | No error logging on texture load failure | `sprite_renderer.c`, `biome.c` |
| **Medium** | ESTATE_DEAD has no death animation before removal | `entities.c:83` |
| **Medium** | `entity.lane` never set — lane-based logic broken | `troop.c` |
| **Medium** | `play_spell` has no in-game effect | `card_effects.c:74` |
| **Medium** | `srand` global PRNG shared by all systems | `tilemap_renderer.c:73` |
| **Medium** | `player_draw_entities` is dead code (never called) | `player.c:101` |
| **Medium** | `biome_define_undead` is dead code | `biome.c:33` |
| **Low** | `atoi` used for card cost — no error handling | `cards.c:42` |
| **Low** | `data` column NULL check missing (unlike `rules_text`) | `cards.c:45` |
| **Low** | `ANIM_RUN` loaded but never used | `sprite_renderer.c` |
| **Low** | No delta-time cap for frame stalls | `game.c:72` |
| **Low** | `energy_full[]` array declared but never populated | `card_renderer.h:88` |
