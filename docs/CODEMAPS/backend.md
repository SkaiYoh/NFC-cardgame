# Backend

## Update Order

`game_update()` currently does exactly this:

1. process debug toggle input
2. if `gameOver` is already latched, tick debug events and return early
3. poll NFC hardware
4. process keyboard debug spawn input
5. update both players' energy and slot cooldown timers
6. update every entity in `Battlefield.entities[]`
7. run `win_check()` as a fallback scan
8. sweep `markedForRemoval` entities backward, clearing stale player base pointers before free
9. tick the debug-event ring buffer

There is no match-phase gate around any of that yet. The game updates directly
in play mode from launch.

## Input Paths

### NFC Path

```text
Arduino serial packet
-> arduino_read_packet()
-> nfc_poll()
-> cards_find_by_uid()
-> card_action_play()
```

### Keyboard Debug Path

```text
1 / 2 / 3 -> Player 0 slot 0 / 1 / 2
Q / W / E -> Player 1 slot 0 / 1 / 2
```

Important caveat:

- the debug spawn path hardcodes `KNIGHT_01`
- that matches the checked-in `cardgame.db`
- it now also matches a fresh database created from `sqlite/seed.sql`

## Card Dispatch

- `card_action_init()` registers eight lower-case card types:
  - `knight`
  - `healer`
  - `assassin`
  - `brute`
  - `farmer`
  - `bird`
  - `fishfing`
  - `king`
- `card_action_play()` dispatches by `Card.type`.
- `spawn_troop_from_card()` performs:
  - slot availability check
  - energy consume
  - canonical spawn position lookup
  - card JSON stat parse
  - troop entity creation
  - lane assignment
  - `waypointIndex = 1`
  - `bf_add_entity()`
- `play_king()` plays on the owning base: checks live base, consumes energy,
  transitions the base into `ESTATE_ATTACKING`, and restarts the hand-card
  animation. Does not spawn a new entity.

## Entity State Machine

`Entity.state` has four live states:

- `ESTATE_IDLE`
  - troops scan for nearby enemies and can transition into `ESTATE_ATTACKING`
- `ESTATE_WALKING`
  - advances through canonical waypoints with `pathfind_step_entity()`
  - checks for nearby enemies and transitions into `ESTATE_ATTACKING`
- `ESTATE_ATTACKING`
  - locks a target by `attackTargetId`
  - advances the attack animation
  - applies damage when the clip crosses its configured hit marker
  - chains the next swing or falls back to walking
- `ESTATE_DEAD`
  - plays the death clip once
  - sets `markedForRemoval` only after the clip finishes

## Combat Model

- Range checks use direct canonical distance with `bf_distance()`.
- `combat_find_target()` iterates the Battlefield registry, skips friendlies,
  and can prioritize buildings when `targeting == TARGET_BUILDING`.
- `TARGET_SPECIFIC_TYPE` currently falls back to nearest-target behavior
  because the entity model has no type-name matcher yet.
- `combat_apply_hit()` is the active troop damage path used by
  `entity_update()`.
- `combat_resolve()` still exists as a legacy cooldown-based helper, but the
  main troop loop does not use it.
- Base kills can latch the match result through either combat helper or
  `building_take_damage()`.

## Base And Win Systems

- `game_init()` spawns one base per player using `bf_base_anchor()` and
  `building_create_base()`.
- Bases are stationary `ENTITY_BUILDING` instances with:
  - `hp = 5000`
  - no attack or movement
  - player-owned base sprite selection and rotation
- `win_trigger()` latches `gameOver` and `winnerID` once.
- `win_latch_from_destroyed_base()` is the authoritative base-destruction
  path.
- `win_check()` is a defensive fallback that can also declare a draw if both
  player bases are already dead.

## Player/Energy Systems

- `player_init()` sets camera state from the canonical territory bounds and
  copies the three spawn anchors into `slots[]`.
- `energy_init()` sets each player to:
  - `maxEnergy = 100.0`
  - `energy = 100.0`
  - `energyRegenRate = 1.5`
- `player_update()` only handles energy regen and slot cooldown countdown.

## Backend Gaps

- `match.c` is unimplemented.
- `projectile.c` is unimplemented.
- Slot cooldowns never activate because nothing sets `cooldownTimer > 0`.
- `spawn.c` is still a placeholder; troop spawn orchestration lives in
  `card_effects.c`.
