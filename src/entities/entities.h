//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_ENTITIES_H
#define NFC_CARDGAME_ENTITIES_H

#include "../core/types.h"

// Lifecycle
Entity *entity_create(EntityType type, Faction faction, Vector2 pos);

void entity_destroy(Entity * e);

// Per-frame
void entity_update(Entity *e, GameState *gs, float deltaTime);

void entity_draw(const Entity *e);

// State transitions
void entity_set_state(Entity *e, EntityState newState);

// Restart the current animation clip without changing state (for chained attacks)
void entity_restart_clip(Entity *e);

#endif //NFC_CARDGAME_ENTITIES_H