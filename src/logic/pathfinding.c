#include "pathfinding.h"
#include "../core/config.h"
#include "../core/battlefield.h"
#include "../entities/entities.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

static bool pathfind_compute_goal(Entity *e, const Battlefield *bf,
                                  Vector2 *outGoal, bool *outIsWaypoint);

BattleSide pathfind_presentation_side_for_position(Vector2 position, float seamY) {
    CanonicalPos pos = { .v = position };
    return bf_side_for_pos(pos, seamY);
}

static float pathfind_entity_sprite_height(const Entity *e) {
    if (!e || !e->sprite) return 0.0f;

    const SpriteSheet *sheet = sprite_sheet_get(e->sprite, e->anim.anim);
    if (!sheet || sheet->frameHeight <= 0) return 0.0f;

    return (float)sheet->frameHeight * e->spriteScale;
}

static BattleSide pathfind_resolved_presentation_side(const Entity *e, const Battlefield *bf) {
    if (!e || !bf) return SIDE_BOTTOM;

    BattleSide resolvedSide = pathfind_presentation_side_for_position(e->position, bf->seamY);
    float spriteHeight = pathfind_entity_sprite_height(e);

    if (spriteHeight > 0.0f) {
        CanonicalPos pos = { .v = e->position };
        if (bf_crosses_seam(pos, spriteHeight, bf->seamY)) {
            resolvedSide = e->presentationSide;
        }
    }

    return resolvedSide;
}

void pathfind_sync_presentation(Entity *e, const Battlefield *bf) {
    if (!e || !bf) return;
    e->spriteRotationDegrees = pathfind_sprite_rotation_for_side(e->anim.dir, e->presentationSide);
}

void pathfind_commit_presentation(Entity *e, const Battlefield *bf) {
    if (!e || !bf) return;

    e->presentationSide = pathfind_resolved_presentation_side(e, bf);
    e->spriteRotationDegrees = pathfind_sprite_rotation_for_side(e->anim.dir, e->presentationSide);
}

void pathfind_face_goal(Entity *e, const Battlefield *bf, Vector2 goal) {
    if (!e || !bf) return;
    Vector2 diff = { goal.x - e->position.x, goal.y - e->position.y };
    float dist = sqrtf(diff.x * diff.x + diff.y * diff.y);
    if (dist <= 1.0f) {
        e->spriteRotationDegrees = pathfind_sprite_rotation_for_side(e->anim.dir,
                                                                     e->presentationSide);
        return;
    }
    pathfind_apply_direction_for_side(&e->anim, diff, e->presentationSide);
    e->spriteRotationDegrees = pathfind_sprite_rotation_for_side(e->anim.dir,
                                                                 e->presentationSide);
}

void pathfind_update_walk_facing(Entity *e, const Battlefield *bf) {
    if (!e || !bf) return;
    Vector2 goal;
    bool goalIsWaypoint = false;
    if (pathfind_compute_goal(e, bf, &goal, &goalIsWaypoint)) {
        pathfind_face_goal(e, bf, goal);
        return;
    }

    if (e->lane < 0 || e->lane >= 3) return;
    BattleSide ownerSide = bf_side_for_player(e->ownerID);
    BattleSide enemySide = (ownerSide == SIDE_BOTTOM) ? SIDE_TOP : SIDE_BOTTOM;
    pathfind_face_goal(e, bf, bf_base_anchor(bf, enemySide).v);
}

// --- Local steering helpers ---

static Vector2 pathfind_rotate_vec(Vector2 v, float degrees) {
    float rad = degrees * PI_F / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);
    return (Vector2){ v.x * c - v.y * s, v.x * s + v.y * c };
}

static float pathfind_point_to_segment_dist(Vector2 p, Vector2 a, Vector2 b) {
    float abx = b.x - a.x;
    float aby = b.y - a.y;
    float apx = p.x - a.x;
    float apy = p.y - a.y;
    float abLenSq = abx * abx + aby * aby;

    if (abLenSq <= 0.0001f) {
        float dx = p.x - a.x;
        float dy = p.y - a.y;
        return sqrtf(dx * dx + dy * dy);
    }

    float t = (apx * abx + apy * aby) / abLenSq;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float closestX = a.x + t * abx;
    float closestY = a.y + t * aby;
    float dx = p.x - closestX;
    float dy = p.y - closestY;
    return sqrtf(dx * dx + dy * dy);
}

static float pathfind_segment_length(Vector2 a, Vector2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return sqrtf(dx * dx + dy * dy);
}

static float pathfind_lane_total_length_for_side(const Battlefield *bf, BattleSide side, int lane) {
    if (!bf) return 0.0f;
    if (lane < 0 || lane >= 3) return 0.0f;

    float total = 0.0f;
    for (int wp = 0; wp < LANE_WAYPOINT_COUNT - 1; wp++) {
        Vector2 a = bf_waypoint(bf, side, lane, wp).v;
        Vector2 b = bf_waypoint(bf, side, lane, wp + 1).v;
        total += pathfind_segment_length(a, b);
    }
    return total;
}

static float pathfind_lane_progress_for_position_on_side(const Battlefield *bf,
                                                         BattleSide side, int lane,
                                                         Vector2 position,
                                                         Vector2 *outClosest) {
    if (!bf) return 0.0f;
    if (lane < 0 || lane >= 3) return 0.0f;

    float bestDistSq = INFINITY;
    float bestProgress = 0.0f;
    Vector2 bestPoint = bf_waypoint(bf, side, lane, 0).v;
    float accumulated = 0.0f;

    for (int wp = 0; wp < LANE_WAYPOINT_COUNT - 1; wp++) {
        Vector2 a = bf_waypoint(bf, side, lane, wp).v;
        Vector2 b = bf_waypoint(bf, side, lane, wp + 1).v;
        float abx = b.x - a.x;
        float aby = b.y - a.y;
        float abLenSq = abx * abx + aby * aby;
        float t = 0.0f;
        Vector2 closest = a;

        if (abLenSq > 0.0001f) {
            float apx = position.x - a.x;
            float apy = position.y - a.y;
            t = (apx * abx + apy * aby) / abLenSq;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            closest.x = a.x + abx * t;
            closest.y = a.y + aby * t;
        }

        float dx = position.x - closest.x;
        float dy = position.y - closest.y;
        float distSq = dx * dx + dy * dy;
        float segmentLength = pathfind_segment_length(a, b);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestProgress = accumulated + segmentLength * t;
            bestPoint = closest;
        }

        accumulated += segmentLength;
    }

    if (outClosest) *outClosest = bestPoint;
    return bestProgress;
}

static Vector2 pathfind_lane_position_at_progress_on_side(const Battlefield *bf,
                                                          BattleSide side, int lane,
                                                          float progress) {
    if (!bf || lane < 0 || lane >= 3) {
        return (Vector2){ 0.0f, 0.0f };
    }

    if (progress <= 0.0f) {
        return bf_waypoint(bf, side, lane, 0).v;
    }

    float remaining = progress;
    for (int wp = 0; wp < LANE_WAYPOINT_COUNT - 1; wp++) {
        Vector2 a = bf_waypoint(bf, side, lane, wp).v;
        Vector2 b = bf_waypoint(bf, side, lane, wp + 1).v;
        float segmentLength = pathfind_segment_length(a, b);

        if (segmentLength <= 0.0001f) continue;
        if (remaining <= segmentLength) {
            float t = remaining / segmentLength;
            return (Vector2){
                a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t
            };
        }

        remaining -= segmentLength;
    }

    return bf_waypoint(bf, side, lane, LANE_WAYPOINT_COUNT - 1).v;
}

static int pathfind_waypoint_index_for_progress_on_side(const Battlefield *bf,
                                                        BattleSide side, int lane,
                                                        float progress) {
    if (!bf || lane < 0 || lane >= 3) return -1;

    float accumulated = 0.0f;
    for (int wp = 1; wp < LANE_WAYPOINT_COUNT; wp++) {
        Vector2 a = bf_waypoint(bf, side, lane, wp - 1).v;
        Vector2 b = bf_waypoint(bf, side, lane, wp).v;
        accumulated += pathfind_segment_length(a, b);
        if (accumulated > progress + 0.001f) {
            return wp;
        }
    }

    return LANE_WAYPOINT_COUNT;
}

float pathfind_lane_progress_for_position(const Entity *e, const Battlefield *bf,
                                          Vector2 position) {
    if (!e || !bf) return 0.0f;
    if (e->lane < 0 || e->lane >= 3) return 0.0f;

    BattleSide ownerSide = bf_side_for_player(e->ownerID);
    return pathfind_lane_progress_for_position_on_side(bf, ownerSide, e->lane, position, NULL);
}

void pathfind_sync_lane_progress(Entity *e, const Battlefield *bf) {
    if (!e || !bf) return;
    if (e->lane < 0 || e->lane >= 3) return;

    BattleSide ownerSide = bf_side_for_player(e->ownerID);
    float projected = pathfind_lane_progress_for_position_on_side(
        bf, ownerSide, e->lane, e->position, NULL
    );
    float total = pathfind_lane_total_length_for_side(bf, ownerSide, e->lane);

    if (projected > e->laneProgress) {
        e->laneProgress = projected;
    }
    if (e->laneProgress < 0.0f) e->laneProgress = 0.0f;
    if (e->laneProgress > total) e->laneProgress = total;

    e->waypointIndex = pathfind_waypoint_index_for_progress_on_side(
        bf, ownerSide, e->lane, e->laneProgress
    );
}

// True if `other` counts as a blocker for `self` during candidate evaluation.
// Living, unmarked troops AND buildings block; projectiles, dead entities,
// and the entity itself do not.
static bool pathfind_is_blocker(const Entity *self, const Entity *other) {
    if (!other || other == self) return false;
    if (!other->alive || other->markedForRemoval) return false;
    if (other->type == ENTITY_PROJECTILE) return false;
    return true;
}

// True if placing `self` at `candidate` would overlap any blocker in `bf`.
// Overlap radius = sum of body radii + PATHFIND_CONTACT_GAP so units stop
// one gap short of actual contact for presentation purposes.
static bool pathfind_overlaps_blocker(const Entity *self, Vector2 candidate,
                                      const Battlefield *bf) {
    if (!self || !bf) return false;
    for (int i = 0; i < bf->entityCount; i++) {
        const Entity *other = bf->entities[i];
        if (!pathfind_is_blocker(self, other)) continue;
        float dx = other->position.x - candidate.x;
        float dy = other->position.y - candidate.y;
        float minDist = self->bodyRadius + other->bodyRadius + PATHFIND_CONTACT_GAP;
        if (dx * dx + dy * dy < minDist * minDist) return true;
    }
    return false;
}

// Resolve the entity's current steering goal.
// Out-params: *outGoal receives the goal position; *outIsWaypoint is true
// when the goal is the lane-following steering point, false when the goal is a
// movementTarget engagement point.
// If movementTargetId is set but the target is invalid, it is cleared here.
// Returns false when there is no remaining steering goal (lane exhausted and
// no valid pursuit target).
static bool pathfind_compute_goal(Entity *e, const Battlefield *bf,
                                  Vector2 *outGoal, bool *outIsWaypoint) {
    if (!e || !bf || !outGoal || !outIsWaypoint) return false;

    if (e->movementTargetId != -1) {
        Entity *target = bf_find_entity((Battlefield *)bf, e->movementTargetId);
        if (target && target->alive && !target->markedForRemoval) {
            *outGoal = target->position;
            *outIsWaypoint = false;
            return true;
        }
        e->movementTargetId = -1;
    }

    if (e->lane < 0 || e->lane >= 3) return false;

    BattleSide ownerSide = bf_side_for_player(e->ownerID);
    pathfind_sync_lane_progress(e, bf);

    float laneTotal = pathfind_lane_total_length_for_side(bf, ownerSide, e->lane);
    if (laneTotal <= 0.0f) return false;
    if (e->laneProgress >= laneTotal - PATHFIND_WAYPOINT_REACH_GAP) return false;

    float goalProgress = e->laneProgress + PATHFIND_LANE_LOOKAHEAD_DISTANCE;
    if (goalProgress > laneTotal) goalProgress = laneTotal;
    *outGoal = pathfind_lane_position_at_progress_on_side(bf, ownerSide, e->lane, goalProgress);
    *outIsWaypoint = true;
    return true;
}

static bool pathfind_candidate_in_bounds(const Entity *e, const Battlefield *bf,
                                         Vector2 candidate) {
    if (!e || !bf) return false;

    return candidate.x - e->bodyRadius >= 0.0f &&
           candidate.x + e->bodyRadius <= bf->boardWidth &&
           candidate.y - e->bodyRadius >= 0.0f &&
           candidate.y + e->bodyRadius <= bf->boardHeight;
}

static bool pathfind_candidate_within_lane_corridor(const Entity *e,
                                                    const Battlefield *bf,
                                                    Vector2 candidate) {
    if (!e || !bf) return false;
    if (e->lane < 0 || e->lane >= 3) return false;

    BattleSide ownerSide = bf_side_for_player(e->ownerID);
    float laneWidth = bf->boardWidth / 3.0f;
    float maxDist = laneWidth * PATHFIND_LANE_DRIFT_MAX_RATIO;
    float bestDist = INFINITY;

    for (int wp = 0; wp < LANE_WAYPOINT_COUNT - 1; wp++) {
        Vector2 a = bf_waypoint(bf, ownerSide, e->lane, wp).v;
        Vector2 b = bf_waypoint(bf, ownerSide, e->lane, wp + 1).v;
        float dist = pathfind_point_to_segment_dist(candidate, a, b);
        if (dist < bestDist) bestDist = dist;
    }

    return bestDist <= maxDist;
}

static float pathfind_candidate_goal_dist(Vector2 goal, Vector2 candidate) {
    float dx = goal.x - candidate.x;
    float dy = goal.y - candidate.y;
    return sqrtf(dx * dx + dy * dy);
}

static float pathfind_candidate_clearance(const Entity *self, Vector2 candidate,
                                          const Battlefield *bf) {
    if (!self || !bf) return -INFINITY;

    float bestClearance = INFINITY;
    for (int i = 0; i < bf->entityCount; i++) {
        const Entity *other = bf->entities[i];
        if (!pathfind_is_blocker(self, other)) continue;

        float dx = other->position.x - candidate.x;
        float dy = other->position.y - candidate.y;
        float centerDist = sqrtf(dx * dx + dy * dy);
        float minDist = self->bodyRadius + other->bodyRadius + PATHFIND_CONTACT_GAP;
        float clearance = centerDist - minDist;
        if (clearance < bestClearance) bestClearance = clearance;
    }

    return (bestClearance == INFINITY) ? 1000000.0f : bestClearance;
}

static bool pathfind_candidate_is_legal(const Entity *e, Vector2 goal,
                                        Vector2 candidate, const Battlefield *bf,
                                        bool enforceLaneCorridor,
                                        float maxGoalDist) {
    if (!pathfind_candidate_in_bounds(e, bf, candidate)) return false;
    if (enforceLaneCorridor &&
        !pathfind_candidate_within_lane_corridor(e, bf, candidate)) return false;
    if (pathfind_overlaps_blocker(e, candidate, bf)) return false;
    return pathfind_candidate_goal_dist(goal, candidate) <= maxGoalDist;
}

static bool pathfind_try_normal_march(Entity *e, Vector2 goal, const Battlefield *bf,
                                      float step, float goalDist,
                                      bool enforceLaneCorridor) {
    float dx = goal.x - e->position.x;
    float dy = goal.y - e->position.y;
    Vector2 forward = { dx / goalDist, dy / goalDist };
    bool preferLeft = ((e->id & 1) == 0);
    float softSign = preferLeft ? -1.0f : 1.0f;
    float hardSign = preferLeft ? -1.0f : 1.0f;
    float sideSign = preferLeft ? -1.0f : 1.0f;

    Vector2 directions[7];
    directions[0] = forward;
    directions[1] = pathfind_rotate_vec(forward,  softSign * PATHFIND_CANDIDATE_ANGLE_SOFT_DEG);
    directions[2] = pathfind_rotate_vec(forward, -softSign * PATHFIND_CANDIDATE_ANGLE_SOFT_DEG);
    directions[3] = pathfind_rotate_vec(forward,  hardSign * PATHFIND_CANDIDATE_ANGLE_HARD_DEG);
    directions[4] = pathfind_rotate_vec(forward, -hardSign * PATHFIND_CANDIDATE_ANGLE_HARD_DEG);
    directions[5] = pathfind_rotate_vec(forward,  sideSign * PATHFIND_CANDIDATE_ANGLE_SIDE_DEG);
    directions[6] = pathfind_rotate_vec(forward, -sideSign * PATHFIND_CANDIDATE_ANGLE_SIDE_DEG);

    for (int i = 0; i < 7; i++) {
        Vector2 cand = {
            e->position.x + directions[i].x * step,
            e->position.y + directions[i].y * step
        };
        if (!pathfind_candidate_is_legal(e, goal, cand, bf, enforceLaneCorridor,
                                         goalDist - 0.001f)) {
            continue;
        }

        e->position = cand;
        return true;
    }

    return false;
}

static bool pathfind_find_best_jam_relief_candidate(const Entity *e, Vector2 goal,
                                                    const Battlefield *bf,
                                                    const Vector2 *directions,
                                                    int directionCount,
                                                    float probeStep,
                                                    bool enforceLaneCorridor,
                                                    float maxGoalDist,
                                                    Vector2 *outCandidate) {
    if (!e || !bf || !directions || directionCount <= 0 || !outCandidate) return false;

    bool found = false;
    Vector2 bestCandidate = e->position;
    float bestClearance = -INFINITY;
    float bestGoalDist = INFINITY;

    for (int i = 0; i < directionCount; i++) {
        Vector2 cand = {
            e->position.x + directions[i].x * probeStep,
            e->position.y + directions[i].y * probeStep
        };
        if (!pathfind_candidate_is_legal(e, goal, cand, bf, enforceLaneCorridor,
                                         maxGoalDist)) {
            continue;
        }

        float clearance = pathfind_candidate_clearance(e, cand, bf);
        float newGoalDist = pathfind_candidate_goal_dist(goal, cand);
        if (!found ||
            clearance > bestClearance + 0.001f ||
            (fabsf(clearance - bestClearance) <= 0.001f &&
             newGoalDist < bestGoalDist)) {
            found = true;
            bestCandidate = cand;
            bestClearance = clearance;
            bestGoalDist = newGoalDist;
        }
    }

    if (found) {
        *outCandidate = bestCandidate;
    }
    return found;
}

static bool pathfind_try_jam_relief(Entity *e, Vector2 goal, const Battlefield *bf,
                                    float step, float goalDist,
                                    bool enforceLaneCorridor) {
    if (!enforceLaneCorridor) return false;
    if (e->ticksSinceProgress < PATHFIND_JAM_RELIEF_TICKS) return false;

    float dx = goal.x - e->position.x;
    float dy = goal.y - e->position.y;
    Vector2 forward = { dx / goalDist, dy / goalDist };
    bool preferLeft = ((e->id & 1) == 0);
    float sideSign = preferLeft ? -1.0f : 1.0f;
    float escapeSign = preferLeft ? -1.0f : 1.0f;

    Vector2 directions[4];
    directions[0] = pathfind_rotate_vec(forward,  sideSign * PATHFIND_CANDIDATE_ANGLE_SIDE_DEG);
    directions[1] = pathfind_rotate_vec(forward, -sideSign * PATHFIND_CANDIDATE_ANGLE_SIDE_DEG);
    directions[2] = pathfind_rotate_vec(forward,  escapeSign * PATHFIND_CANDIDATE_ANGLE_ESCAPE_DEG);
    directions[3] = pathfind_rotate_vec(forward, -escapeSign * PATHFIND_CANDIDATE_ANGLE_ESCAPE_DEG);

    // Probe progressively wider escape radii, but still keep the first
    // radius that yields any legal non-overlapping escape so large jumps stay
    // a fallback for dense jams rather than the default movement mode.
    //
    // Slow real troops only move about 1 px per frame, which is far smaller
    // than the ~30 px non-overlap shell between adjacent bodies. Start the
    // jam probe at least one body-radius out so dense packs can search beyond
    // sub-pixel dead zones while preserving the same hard legality checks.
    float baseProbeStep = fmaxf(step, e->bodyRadius);
    static const float probeStepMultipliers[] = { 1.0f, 2.0f, 3.0f };
    for (int i = 0; i < (int)(sizeof(probeStepMultipliers) / sizeof(probeStepMultipliers[0])); i++) {
        float probeStep = baseProbeStep * probeStepMultipliers[i];
        float maxGoalDist = goalDist + PATHFIND_ESCAPE_BACKTRACK_STEP_RATIO * probeStep;
        Vector2 bestCandidate;
        if (pathfind_find_best_jam_relief_candidate(
                e, goal, bf, directions, 4, probeStep,
                enforceLaneCorridor, maxGoalDist, &bestCandidate)) {
            e->position = bestCandidate;
            return true;
        }
    }

    return false;
}

// Try one obstacle-aware step toward `goal`. Updates position, facing, and
// ticksSinceProgress. Returns true if a candidate was accepted and the
// entity moved; false if nothing was valid. Shared between pathfind_step_entity
// (lane waypoints / engagement goals) and pathfind_move_toward_goal
// (farmers / other goal-based callers).
static bool pathfind_try_step_toward(Entity *e, Vector2 goal,
                                     const Battlefield *bf, float deltaTime,
                                     bool allowJamRelief,
                                     bool enforceLaneCorridor) {
    float dx = goal.x - e->position.x;
    float dy = goal.y - e->position.y;
    float goalDist = sqrtf(dx * dx + dy * dy);

    bool moved = false;

    if (goalDist > 0.001f) {
        float step = e->moveSpeed * deltaTime;
        // Cap step so the entity doesn't hop through the goal in a single frame.
        if (step > goalDist) step = goalDist;
        if (step > 0.0f) {
            moved = pathfind_try_normal_march(e, goal, bf, step, goalDist,
                                              enforceLaneCorridor);
            if (!moved && allowJamRelief) {
                moved = pathfind_try_jam_relief(e, goal, bf, step, goalDist,
                                                enforceLaneCorridor);
            }
        }
    }

    if (moved) {
        e->ticksSinceProgress = 0;
        pathfind_sync_presentation(e, bf);
        pathfind_face_goal(e, bf, goal);
    } else {
        e->ticksSinceProgress++;
    }
    return moved;
}

bool pathfind_move_toward_goal(Entity *e, Vector2 goal, float stopRadius,
                               const Battlefield *bf, float deltaTime) {
    if (!e || !bf) return true;

    float dx = goal.x - e->position.x;
    float dy = goal.y - e->position.y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist <= stopRadius) {
        e->ticksSinceProgress = 0;
        return true;
    }

    pathfind_try_step_toward(e, goal, bf, deltaTime, false, false);
    return false;
}

bool pathfind_step_entity(Entity *e, const Battlefield *bf, float deltaTime) {
    // Debug assertion: entity position must be within canonical board bounds
    CanonicalPos posCheck = { e->position };
    BF_ASSERT_IN_BOUNDS(posCheck, BOARD_WIDTH, BOARD_HEIGHT);

    // Validate lane bounds -- invalid lane means entity cannot path
    if (e->lane < 0 || e->lane >= 3) {
        entity_set_state(e, ESTATE_IDLE);
        return false;
    }

    BattleSide ownerSide = bf_side_for_player(e->ownerID);

    // --- Resolve steering goal ---
    Vector2 goal;
    bool goalIsWaypoint = false;
    if (!pathfind_compute_goal(e, bf, &goal, &goalIsWaypoint)) {
        BattleSide enemySide = (ownerSide == SIDE_BOTTOM) ? SIDE_TOP : SIDE_BOTTOM;
        pathfind_commit_presentation(e, bf);
        pathfind_face_goal(e, bf, bf_base_anchor(bf, enemySide).v);
        entity_set_state(e, ESTATE_IDLE);
        return false;
    }

    pathfind_try_step_toward(e, goal, bf, deltaTime, true, true);
    pathfind_sync_lane_progress(e, bf);

    // --- Lane bookkeeping (only meaningful when lane-following) ---
    if (goalIsWaypoint) {
        float laneTotal = pathfind_lane_total_length_for_side(bf, ownerSide, e->lane);
        if (e->laneProgress >= laneTotal - PATHFIND_WAYPOINT_REACH_GAP) {
            // End of path: idle in place, face enemy base. No random jitter.
            BattleSide enemySide = (ownerSide == SIDE_BOTTOM) ? SIDE_TOP : SIDE_BOTTOM;
            CanonicalPos enemyBase = bf_base_anchor(bf, enemySide);
            pathfind_commit_presentation(e, bf);
            pathfind_face_goal(e, bf, enemyBase.v);
            entity_set_state(e, ESTATE_IDLE);
            return false;
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

    // Vertical facing is side-perspective aware:
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
