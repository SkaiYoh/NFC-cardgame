//
// Created by Nathan Davis on 2/16/26.
//

#include "pathfinding.h"
#include "../core/types.h"

// TODO: Pathfinding is completely unimplemented — only a forward declaration exists here.
// TODO: Currently entities move in a straight line toward their goal (or not at all, since
// TODO: entity_update has no movement code). Implement pathfind_next_step() to return the
// TODO: next world-space position a troop should move toward this frame. A minimal approach:
// TODO:   - Normalize (goal - current), multiply by moveSpeed * deltaTime → return current + step
// TODO: A more complete approach would use lane waypoints defined per-player so troops follow
// TODO: a predefined path rather than walking diagonally across the tilemap.
// TODO: Call pathfind_next_step from entity_update when ESTATE_WALKING.
Vector2 pathfind_next_step(Vector2 current, Vector2 goal, GameState *gs);