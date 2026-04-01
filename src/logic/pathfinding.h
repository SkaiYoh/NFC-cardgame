#ifndef NFC_CARDGAME_PATHFINDING_H
#define NFC_CARDGAME_PATHFINDING_H

#include "../core/types.h"
#include "../core/battlefield.h"

// Advance entity one frame along canonical Battlefield waypoints.
// Returns true if entity is still walking, false if it reached the end (transitioned to IDLE).
bool pathfind_step_entity(Entity *e, const Battlefield *bf, float deltaTime);

// Apply waypoint-based facing to an animation state.
// Horizontal movement uses DIR_SIDE with flipH; vertical uses DIR_UP / DIR_DOWN.
// diff: vector from current position toward target waypoint (target - position).
void pathfind_apply_direction(AnimState *anim, Vector2 diff);

#endif //NFC_CARDGAME_PATHFINDING_H
