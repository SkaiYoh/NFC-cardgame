//
// Created by Nathan Davis on 2/16/26.
//

#include "projectile.h"

// TODO: Projectile system is completely unimplemented. This file is empty.
// TODO: Projectiles are needed for ranged card types (archers, spell effects, area attacks).
// TODO: Implement:
//   projectile_create() — allocate and initialize a projectile entity with velocity and a target
//   projectile_update() — move toward target each frame, detect collision with enemy entities
//   projectile_resolve() — apply damage on hit, mark projectile for removal
// TODO: Projectiles should be tracked in Player.entities[] as ENTITY_PROJECTILE or in a separate
// TODO: pool to avoid mixing them with troops in entity_update logic.