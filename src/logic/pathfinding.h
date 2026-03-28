#ifndef NFC_CARDGAME_PATHFINDING_H
#define NFC_CARDGAME_PATHFINDING_H

#include "../core/types.h"

// Generate pre-computed waypoints for all 3 lanes for a player.
// Must be called after player_init_card_slots (uses slot worldPos for waypoint[0]).
// Stores results in p->laneWaypoints[3][LANE_WAYPOINT_COUNT].
void lane_generate_waypoints(Player *p);

// Advance entity one frame along its lane waypoints.
// Returns true if entity is still walking, false if it reached the end (transitioned to IDLE).
// Handles: movement toward current target waypoint, waypoint advancement on arrival,
// end-of-path jitter and ESTATE_IDLE transition.
// NOTE: Does NOT update sprite direction -- call pathfind_compute_direction() separately.
bool pathfind_step_entity(Entity *e, const Player *owner, float deltaTime);

// Update entity sprite direction (anim.dir, anim.flipH) from the movement vector.
// Uses dominant-axis with hysteresis to prevent flicker near 45-degree angles.
// diff: vector from entity position toward target waypoint (target - position).
void pathfind_compute_direction(Entity *e, Vector2 diff);

#endif //NFC_CARDGAME_PATHFINDING_H
