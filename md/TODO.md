# NFC Card Game — Project To-Do List

## Implementation Status

### Done

| Feature | Files |
|---------|-------|
| Database layer (PostgreSQL connection, card loading) | `src/data/db.c`, `cards.c` |
| Card effect handler registry | `src/logic/card_effects.c` |
| Card effects → troop spawning (all 5 types) | `src/logic/card_effects.c`, `src/entities/troop.c` |
| Entity lifecycle (create/update/draw/destroy, state machine) | `src/entities/entities.c` |
| Entity movement (straight-line walk, border crossing, despawn) | `src/entities/entities.c` |
| Sprite animation system (5 character types, all anim types) | `src/rendering/sprite_renderer.c` |
| Card visual rendering (13 colors, 4 bg styles, layer offsets) | `src/rendering/card_renderer.c` |
| Procedural tilemap generation (biome-aware) | `src/rendering/tilemap_renderer.c`, `biome.c` |
| Split-screen viewports (±90° camera, scissor clipping) | `src/rendering/viewport.c` |
| Player setup (camera, 3 card slots, entity list, energy regen) | `src/systems/player.c` |
| Core game loop (init → update → render → cleanup) | `src/core/game.c` |

---

## Production Flow

End-to-end system from physical hardware to rendered frame.

```
HARDWARE LAYER
  NFC Reader (×3 per player) → Arduino Nano
    → Serial USB → nfc_reader.c → arduino_protocol.c
    → Card UID string → db lookup → Card* pointer

PREGAME PHASE  [match.c — stub]
  pregame_init()                    → allocate per-match state
  pregame_handle_card_scan() ×n    → each player scans their deck cards
  pregame_are_players_ready()      → both ready → start match
  pregame_update/render()          → lobby countdown screen

IN-GAME LOOP  [game.c]
  game_update(deltaTime)
  ├── NFC scan event → card_action_play(card, gs)           [card_effects.c]
  │     └── Handler dispatch by card->type
  │           ├── Troop card → spawn_troop_from_card()
  │           │     ├── energy_can_afford()  → reject if < cost
  │           │     ├── energy_consume()     → deduct cost
  │           │     ├── troop_create_data_from_card() → parse JSON stats
  │           │     ├── troop_spawn()        → entity_create() + player_add_entity()
  │           │     └── slot cooldown starts
  │           └── Spell card → apply effect (damage, buff, debuff)
  │
  ├── player_update() ×2                                     [player.c]
  │     ├── energy_update()         → regen energy (capped at maxEnergy)
  │     └── slot cooldown decrement
  │
  ├── player_update_entities() ×2                            [entities.c]
  │     └── entity_update() per entity
  │           ├── WALKING → move Y; combat_find_target()     [combat.c]
  │           │     └── target in attackRange → transition ATTACKING
  │           ├── ATTACKING → combat_resolve(attacker, target)
  │           │     ├── target.hp -= attacker.attack
  │           │     └── target.hp ≤ 0 → entity_set_state(DEAD)
  │           └── DEAD → markedForRemoval = true (after death anim)
  │
  └── win_check(gs)                                          [win_condition.c]
        └── base.hp ≤ 0 → win_trigger() → end-game state

  game_render()
  ├── Viewport P1 (scissor left 960px, Camera2D +90°)
  │     ├── viewport_draw_tilemap()
  │     ├── game_draw_entities_for_viewport()  (owner + crossed opponents mirrored)
  │     └── ui_draw_hud(player 1)              energy bar, base HP, card slots
  └── Viewport P2 (scissor right 960px, Camera2D -90°)
        ├── viewport_draw_tilemap()
        ├── game_draw_entities_for_viewport()
        └── ui_draw_hud(player 2)

END-GAME STATE  [win_condition.c — stub]
  win_trigger()  → freeze game, show winner overlay, prompt rematch
  win_check()    → called each frame, monitors both bases' HP
```

---

## Remaining Work

### Critical (no game without these)

| # | Feature | Files | Notes |
|---|---------|-------|-------|
| 1 | **Combat system** — targeting, damage, attack resolution | `src/logic/combat.c` | 3 bare declarations; no includes, no implementation |
| 2 | **Win conditions** — base HP tracking, victory/defeat detection | `src/logic/win_condition.c` | 2 bare declarations; no implementation |
| 3 | **Energy enforcement** — check cost before card play, consume on play | `src/systems/energy.c`, `card_effects.c` | Regen works; `can_afford`/`consume` stubs not wired up |
| 4 | **Building system** — base entity with HP, takes damage from crossed troops | `src/entities/building.c` | Two no-op stubs; base HP never tracked |

### Important (needed for real gameplay)

| # | Feature | Files | Notes |
|---|---------|-------|-------|
| 5 | **UI/HUD** — energy bar, base HP bars, active card slot display | `src/rendering/ui.c` | Empty module |
| 6 | **Attack animation state** — `ESTATE_ATTACKING` in entity state machine | `src/entities/entities.c`, `types.h` | Currently only IDLE/WALKING/DEAD |
| 7 | **Spell card effects** — actual damage/buff logic (not just printf) | `src/logic/card_effects.c` | `play_spell()` prints debug only |
| 8 | **Projectile system** — spawn projectile entity, fly toward target | `src/entities/projectile.c` | Completely empty |

### Nice to Have (polish & full experience)

| # | Feature | Files | Notes |
|---|---------|-------|-------|
| 9 | **Pregame/match lobby** — player readiness, card selection phase | `src/systems/match.c` | 5 bare declarations |
| 10 | **NFC hardware integration** — Arduino serial protocol, card UID → Card* | `src/hardware/nfc_reader.c`, `arduino_protocol.c` | Completely empty |
| 11 | **Biome completion** — Snow and Swamp biomes (currently reuse Grass) | `src/rendering/biome.c` | Placeholder implementations |
| 12 | **Makefile sync** — Add all stub modules to SOURCES (matches CMake) | `Makefile` | See below |

---

## Implementation Order (remaining work)

1. **Makefile sync** — add missing modules so `make cardgame` matches `cmake`
2. **Building system** — give each player a base entity with HP; when a troop despawns after crossing, deal damage to opponent's base
3. **Energy enforcement** — implement `energy_can_afford()` / `energy_consume()` in `energy.c`; call from `spawn_troop_from_card()`
4. **Combat system** — `combat_find_target()` (nearest enemy in lane), `combat_in_range()`, `combat_resolve()` (deal damage)
5. **Attack state** — add `ESTATE_ATTACKING` to state machine; entities stop walking and play attack animation
6. **Win conditions** — `win_check()` scans bases each frame; `win_trigger()` freezes the game
7. **UI/HUD** — energy bar, base HP bars, card slot cooldown indicators
8. **Spell effects** — implement actual damage/buff logic in `play_spell()`
9. **Pregame lobby** — scan-in phase before match begins
10. **NFC hardware** — Arduino serial communication, card UID lookup

Roughly **600–900 lines** of gameplay code remain to reach a fully playable prototype.
