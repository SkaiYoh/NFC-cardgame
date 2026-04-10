//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_COMBAT_H
#define NFC_CARDGAME_COMBAT_H

#include "../core/types.h"
#include <stdbool.h>

// Returns true if b is within a's attack range (cross-space aware)
bool combat_in_range(const Entity *a, const Entity *b, const GameState *gs);

// Find the best target for attacker among the enemy player's entities
Entity *combat_find_target(Entity *attacker, GameState *gs);

// Enemy-only nearest-valid probe bounded by maxRadius (center-to-center).
// Used by the local steering aggro probe. Unlike combat_find_target, this
// does NOT run the heal-first branch: walking healers do not chase injured
// allies. TARGET_BUILDING priority is still preserved for building-focused
// attackers whose nearest building falls inside maxRadius.
Entity *combat_find_target_within_radius(Entity *attacker, GameState *gs, float maxRadius);

// Apply one attack from attacker to target (respects cooldown).
// Legacy: used before clip-driven attacks. Retained for non-clip entities.
void combat_resolve(Entity *attacker, Entity *target, GameState *gs, float deltaTime);

// Apply one hit from attacker to target (no cooldown check, immediate damage).
// Called by the animation hit-sync system when the attack clip crosses its hit marker.
// Checks for base kill and latches win condition if applicable.
void combat_apply_hit(Entity *attacker, Entity *target, GameState *gs);

// Apply damage to an entity; transitions to ESTATE_DEAD if hp <= 0.
// Returns true if the entity was just killed (was alive, now dead).
bool entity_take_damage(Entity *entity, int damage);

// Returns true if target is a currently valid friendly heal target for attacker.
// Requires a supporter unit, another living friendly troop, and hp < maxHP.
bool combat_can_heal_target(const Entity *attacker, const Entity *target);

// Restore HP to an entity, clamped to maxHP. No-op on dead/marked entities.
// Returns true if any HP was actually restored.
bool entity_apply_heal(Entity *entity, int amount);

#endif //NFC_CARDGAME_COMBAT_H
