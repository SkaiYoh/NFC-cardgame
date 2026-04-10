//
// Created by Nathan Davis on 2/16/26.
//

#include "combat.h"
#include "farmer.h"
#include "win_condition.h"
#include "../core/battlefield.h"
#include "../core/battlefield_math.h"
#include "../core/debug_events.h"
#include "../entities/entities.h"
#include <math.h>
#include <float.h>
#include <stdio.h>

// All positions are now canonical -- direct distance via bf_distance (per D-18).
// Cross-space mapping and per-axis distance helpers have been deleted.

bool combat_in_range(const Entity *a, const Entity *b, const GameState *gs) {
    (void)gs;  // No longer needs GameState for coordinate mapping
    if (!a || !b) return false;

    // Both positions are canonical -- direct distance (per D-18)
    CanonicalPos posA = { a->position };
    CanonicalPos posB = { b->position };
    return bf_distance(posA, posB) <= a->attackRange;
}

static bool combat_is_friendly_target(const Entity *attacker, const Entity *target) {
    if (!attacker || !target) return false;
    return target->ownerID == attacker->ownerID;
}

bool combat_can_heal_target(const Entity *attacker, const Entity *target) {
    if (!attacker || !target) return false;
    if (attacker->healAmount <= 0) return false;
    if (target == attacker) return false;
    if (!combat_is_friendly_target(attacker, target)) return false;
    if (target->type != ENTITY_TROOP) return false;
    if (!target->alive || target->markedForRemoval) return false;
    if (target->hp >= target->maxHP) return false;
    return true;
}

// Healer priority: nearest injured friendly troop already inside attack range.
// Returns NULL if no eligible ally exists; the caller then falls back to enemy targeting.
static Entity *combat_find_heal_target(Entity *attacker, GameState *gs) {
    Battlefield *bf = &gs->battlefield;
    Entity *bestTarget = NULL;
    float bestDist = FLT_MAX;
    CanonicalPos attackerPos = { attacker->position };

    for (int i = 0; i < bf->entityCount; i++) {
        Entity *candidate = bf->entities[i];
        if (!combat_can_heal_target(attacker, candidate)) continue;

        CanonicalPos candidatePos = { candidate->position };
        float d = bf_distance(attackerPos, candidatePos);
        if (d > attacker->attackRange) continue;                // already in range only

        if (d < bestDist) {
            bestDist = d;
            bestTarget = candidate;
        }
    }
    return bestTarget;
}

// Shared enemy-scan helper. Returns the nearest valid enemy within maxRadius
// (center-to-center canonical distance), honoring the attacker's targeting
// mode (nearest-valid, with TARGET_BUILDING priority). Pass FLT_MAX for an
// unlimited search. Does NOT run the heal-first branch -- callers that need
// heal-first semantics must run combat_find_heal_target themselves.
static Entity *combat_find_enemy_within(Entity *attacker, GameState *gs, float maxRadius) {
    Battlefield *bf = &gs->battlefield;
    Entity *bestTarget = NULL;
    float bestDist = FLT_MAX;
    CanonicalPos attackerPos = { attacker->position };

    for (int i = 0; i < bf->entityCount; i++) {
        Entity *candidate = bf->entities[i];
        if (!candidate->alive || candidate->markedForRemoval) continue;
        if (candidate->ownerID == attacker->ownerID) continue; // skip friendlies

        CanonicalPos candidatePos = { candidate->position };
        float d = bf_distance(attackerPos, candidatePos);
        if (d > maxRadius) continue;

        switch (attacker->targeting) {
            case TARGET_BUILDING:
                if (candidate->type == ENTITY_BUILDING) {
                    if (d < bestDist || (bestTarget && bestTarget->type != ENTITY_BUILDING)) {
                        bestDist = d;
                        bestTarget = candidate;
                    }
                    continue;
                }
                // Fall through to nearest for non-buildings as fallback
                // fallthrough
            case TARGET_NEAREST:
            case TARGET_SPECIFIC_TYPE: // No name field on Entity yet -- falls back to nearest
                if (d < bestDist || bestTarget == NULL) {
                    if (attacker->targeting == TARGET_BUILDING && bestTarget &&
                        bestTarget->type == ENTITY_BUILDING) {
                        // Don't replace a building target with a non-building
                        continue;
                    }
                    if (d < bestDist) {
                        bestDist = d;
                        bestTarget = candidate;
                    }
                }
                break;
        }
    }
    return bestTarget;
}

Entity *combat_find_target(Entity *attacker, GameState *gs) {
    if (!attacker || !gs) return NULL;

    if (attacker->healAmount > 0) {
        Entity *ally = combat_find_heal_target(attacker, gs);
        if (ally) return ally;
        // No injured ally in range -- fall through to normal enemy targeting.
    }

    // TODO: base fallback needs rework when bases are in Battlefield entity registry
    return combat_find_enemy_within(attacker, gs, FLT_MAX);
}

Entity *combat_find_target_within_radius(Entity *attacker, GameState *gs, float maxRadius) {
    if (!attacker || !gs) return NULL;
    if (maxRadius < 0.0f) return NULL;
    return combat_find_enemy_within(attacker, gs, maxRadius);
}

bool entity_take_damage(Entity *entity, int damage) {
    if (!entity || !entity->alive) return false;

    entity->hp -= damage;
    if (entity->hp <= 0) {
        entity->hp = 0;
        entity->alive = false;
        entity_set_state(entity, ESTATE_DEAD);
        return true;
    }
    return false;
}

bool entity_apply_heal(Entity *entity, int amount) {
    if (!entity || !entity->alive || entity->markedForRemoval) return false;
    if (amount <= 0) return false;
    if (entity->hp >= entity->maxHP) return false;

    int before = entity->hp;
    entity->hp += amount;
    if (entity->hp > entity->maxHP) entity->hp = entity->maxHP;
    return entity->hp > before;
}

// Handle post-kill consequences for any entity type.
// Currently handles farmer sustenance transfer; extend for future unit roles.
static void combat_on_kill(Entity *victim, GameState *gs) {
    if (victim->unitRole == UNIT_ROLE_FARMER) {
        farmer_on_death(victim, gs);
    }
}

typedef enum {
    EFFECT_NONE,
    EFFECT_HEAL,
    EFFECT_DAMAGE
} EffectResult;

static bool combat_is_invalid_supporter_friendly_target(const Entity *attacker, const Entity *target) {
    if (!attacker || !target) return false;
    if (attacker->healAmount <= 0) return false;
    if (!combat_is_friendly_target(attacker, target)) return false;
    return !combat_can_heal_target(attacker, target);
}

// Apply one effect (heal or damage) from attacker to target. Single choke point
// shared by combat_apply_hit (clip-driven) and combat_resolve (legacy cooldown)
// so healer semantics stay uniform across the public combat API.
static EffectResult apply_effect(Entity *attacker, Entity *target, GameState *gs) {
    if (combat_can_heal_target(attacker, target)) {
        entity_apply_heal(target, attacker->healAmount);
        debug_event_emit_xy(target->position.x, target->position.y, DEBUG_EVT_HIT);
        printf("[COMBAT] Entity %d healed entity %d for %d (hp: %d/%d)\n",
               attacker->id, target->id, attacker->healAmount, target->hp, target->maxHP);
        return EFFECT_HEAL;
    }

    if (combat_is_invalid_supporter_friendly_target(attacker, target)) {
        return EFFECT_NONE;
    }

    bool killed = entity_take_damage(target, attacker->attack);
    debug_event_emit_xy(target->position.x, target->position.y, DEBUG_EVT_HIT);

    printf("[COMBAT] Entity %d dealt %d damage to entity %d (hp: %d/%d)\n",
           attacker->id, attacker->attack, target->id, target->hp, target->maxHP);

    if (killed) {
        combat_on_kill(target, gs);
        if (target->type == ENTITY_BUILDING) {
            win_latch_from_destroyed_base(gs, target);
        }
    }
    return EFFECT_DAMAGE;
}

void combat_apply_hit(Entity *attacker, Entity *target, GameState *gs) {
    if (!attacker || !target || !gs) return;
    if (!target->alive) return;

    apply_effect(attacker, target, gs);
}

void combat_resolve(Entity *attacker, Entity *target, GameState *gs, float deltaTime) {
    if (!attacker || !target || !gs) return;
    if (!attacker->alive || attacker->markedForRemoval) return;
    if (!target->alive || target->markedForRemoval) return;

    // Tick cooldown
    attacker->attackCooldown -= deltaTime;
    if (attacker->attackCooldown < 0.0f) attacker->attackCooldown = 0.0f;

    // Not ready to attack yet
    if (attacker->attackCooldown > 0.0f) return;

    EffectResult result = apply_effect(attacker, target, gs);
    if (result != EFFECT_NONE) {
        attacker->attackCooldown = (attacker->attackSpeed > 0.0f)
            ? 1.0f / attacker->attackSpeed
            : 1.0f;
    }
}
