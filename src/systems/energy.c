//
// Created by Nathan Davis on 2/16/26.
//

#include "energy.h"
#include "../core/types.h"

// TODO: All energy functions are forward-declared here but none are implemented — this file
// TODO: contains only declarations, not definitions. The actual energy logic lives inline in
// TODO: player.c (player_update increments p->energy directly). Move that logic here and
// TODO: implement each function body:
// TODO:   energy_init()      — set p->maxEnergy, p->energy, p->energyRegen
// TODO:   energy_update()    — p->energy = fminf(p->energy + p->energyRegen * dt, p->maxEnergy)
// TODO:   energy_can_afford()— return p->energy >= cost
// TODO:   energy_consume()   — deduct cost, return false if insufficient (currently never called)
// TODO:   energy_restore()   — add amount, clamp to maxEnergy
// TODO:   energy_set_regen_rate() — update p->energyRegen for buff/debuff effects
void energy_init(Player *p, float maxEnergy, float regenRate);
void energy_update(Player *p, float deltaTime);
bool energy_can_afford(Player *p, int cost);
bool energy_consume(Player *p, int cost);
void energy_restore(Player *p, float amount);
void energy_set_regen_rate(Player *p, float newRate);  // For buffs

