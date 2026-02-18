//
// Created by Nathan Davis on 2/16/26.
//

#include "combat.h"
#include "../core/types.h"

void combat_resolve(Entity *attacker, Entity *target);
bool combat_in_range(Entity *a, Entity *b);
Entity* combat_find_target(Entity *attacker, GameState *gs);