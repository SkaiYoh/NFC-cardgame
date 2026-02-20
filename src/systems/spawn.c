//
// Created by Nathan Davis on 2/16/26.
//

#include "spawn.h"

// TODO: spawn.c is completely empty. The current spawn path goes through card_effects.c →
// TODO: spawn_troop_from_card() (defined inline in player.c), bypassing this module entirely.
// TODO: Consolidate troop/building spawn logic here:
// TODO:   spawn_troop()    — wraps troop_spawn, assigns e->lane, adds to player->entities[]
// TODO:   spawn_building() — wraps building_create_base/building_create (not yet implemented),
// TODO:                      assigns to player->base or a building slot
// TODO: Having spawn logic centralized here makes it easier to add spawn animations, sound
// TODO: effects, and spawn-rate limiting (e.g. cooldown between spawns on the same lane).