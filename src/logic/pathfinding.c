#include "pathfinding.h"
#include "../core/config.h"
#include "../core/battlefield.h"
#include "../entities/entities.h"
#include <math.h>
#include <stdlib.h>

bool pathfind_step_entity(Entity *e, const Battlefield *bf, float deltaTime) {
    // Debug assertion: entity position must be within canonical board bounds
    CanonicalPos posCheck = { e->position };
    BF_ASSERT_IN_BOUNDS(posCheck, BOARD_WIDTH, BOARD_HEIGHT);

    // Validate lane bounds -- invalid lane means entity cannot path
    if (e->lane < 0 || e->lane >= 3) {
        entity_set_state(e, ESTATE_IDLE);
        return false;
    }

    // Check if already past all waypoints
    if (e->waypointIndex >= LANE_WAYPOINT_COUNT) {
        entity_set_state(e, ESTATE_IDLE);
        return false;
    }

    // Read canonical waypoint from Battlefield (per D-05, D-18)
    BattleSide side = bf_side_for_player(e->ownerID);
    CanonicalPos targetWP = bf_waypoint(bf, side, e->lane, e->waypointIndex);
    Vector2 target = targetWP.v;
    float dx = target.x - e->position.x;
    float dy = target.y - e->position.y;
    float dist = sqrtf(dx * dx + dy * dy);
    float step = e->moveSpeed * deltaTime;
    bool advancedWaypoint = false;

    if (dist <= step || dist < 1.0f) {
        // Reached waypoint -- snap and advance
        e->position = target;
        e->waypointIndex++;
        advancedWaypoint = true;

        if (e->waypointIndex >= LANE_WAYPOINT_COUNT) {
            // End of path: apply random jitter (per D-11)
            float jx = ((float) (rand() % ((int) (LANE_JITTER_RADIUS * 2) + 1)) - LANE_JITTER_RADIUS);
            float jy = ((float) (rand() % ((int) (LANE_JITTER_RADIUS * 2) + 1)) - LANE_JITTER_RADIUS);
            e->position.x += jx;
            e->position.y += jy;
            // Face toward the enemy when idling at the end of the path.
            // From either owner's perspective, reaching the enemy side means
            // the unit is facing away from its home camera.
            e->anim.dir = DIR_UP;
            e->anim.flipH = false;
            e->spriteRotationDegrees = pathfind_sprite_rotation_for_side(e->anim.dir, side);
            entity_set_state(e, ESTATE_IDLE);
            return false;
        }
    } else {
        // Move toward waypoint
        float inv = 1.0f / dist;
        e->position.x += dx * inv * step;
        e->position.y += dy * inv * step;
    }

    // Only update facing during actual movement -- not on the snap frame
    // where the entity just arrived at a waypoint. This preserves the
    // walking direction so entities don't snap to a new facing on arrival.
    if (!advancedWaypoint) {
        Vector2 diff = {target.x - e->position.x, target.y - e->position.y};
        float ddist = sqrtf(diff.x * diff.x + diff.y * diff.y);
        if (ddist > 1.0f) {
            pathfind_apply_direction_for_side(&e->anim, diff, side);
            e->spriteRotationDegrees = pathfind_sprite_rotation_for_side(e->anim.dir, side);
        }
    }

    return true;
}

void pathfind_apply_direction(AnimState *anim, Vector2 diff) {
    const float eps = 0.001f;

    if (fabsf(diff.x) < eps && fabsf(diff.y) < eps) return;

    if (fabsf(diff.x) >= eps) {
        anim->dir = DIR_SIDE;
        anim->flipH = (diff.x < 0);
    } else if (diff.y < 0) {
        anim->dir = DIR_UP;
        anim->flipH = false;
    } else {
        anim->dir = DIR_DOWN;
        anim->flipH = false;
    }
}

void pathfind_apply_direction_for_side(AnimState *anim, Vector2 diff, BattleSide side) {
    const float eps = 0.001f;

    if (fabsf(diff.x) < eps && fabsf(diff.y) < eps) return;

    if (fabsf(diff.x) >= eps) {
        anim->dir = DIR_SIDE;
        // Top-side units are rendered with a 180-degree sprite rotation, so
        // their horizontal flip must be inverted to preserve left/right facing.
        anim->flipH = (side == SIDE_TOP) ? (diff.x > 0) : (diff.x < 0);
        return;
    }

    // Vertical facing is owner-perspective aware:
    // SIDE_BOTTOM: moving up the canonical board means away from camera.
    // SIDE_TOP: moving down the canonical board means away from camera.
    bool movingAway = (side == SIDE_BOTTOM) ? (diff.y < 0.0f) : (diff.y > 0.0f);
    anim->dir = movingAway ? DIR_UP : DIR_DOWN;
    anim->flipH = false;
}

float pathfind_sprite_rotation_for_side(SpriteDirection dir, BattleSide side) {
    (void)dir;
    if (side == SIDE_TOP) {
        return 180.0f;
    }
    return 0.0f;
}
