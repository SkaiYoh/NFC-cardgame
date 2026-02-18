//
// Created by Nathan Davis on 2/16/26.
//

#include "entities.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int s_nextEntityID = 1;

Entity *entity_create(EntityType type, Faction faction, Vector2 pos) {
    Entity *e = malloc(sizeof(Entity));
    if (!e) return NULL;
    memset(e, 0, sizeof(Entity));

    e->id = s_nextEntityID++;
    e->type = type;
    e->faction = faction;
    e->position = pos;
    e->state = ESTATE_IDLE;
    e->alive = true;
    e->markedForRemoval = false;
    e->spriteScale = 2.0f;

    anim_state_init(&e->anim, ANIM_IDLE, DIR_UP, 8.0f);

    return e;
}

void entity_destroy(Entity *e) {
    if (e) free(e);
}

void entity_set_state(Entity *e, EntityState newState) {
    if (!e || e->state == newState) return;

    EntityState oldState = e->state;
    e->state = newState;

    // Reset animation on state change
    switch (newState) {
        case ESTATE_IDLE:
            anim_state_init(&e->anim, ANIM_IDLE, e->anim.dir, 8.0f);
            break;
        case ESTATE_WALKING:
            anim_state_init(&e->anim, ANIM_WALK, e->anim.dir, 10.0f);
            break;
        case ESTATE_DEAD:
            anim_state_init(&e->anim, ANIM_DEATH, e->anim.dir, 8.0f);
            break;
    }

    (void)oldState;
}

void entity_update(Entity *e, GameState *gs, float deltaTime) {
    if (!e || !e->alive) return;

    switch (e->state) {
        case ESTATE_IDLE:
            // Just animate in place
            break;

        case ESTATE_WALKING: {
            Player *owner = &gs->players[e->ownerID];

            // Move forward (decreasing Y) through own area and into opponent's
            e->position.y -= e->moveSpeed * deltaTime;

            // Despawn when entity reaches the opponent's base depth
            float despawnY = owner->playArea.y - owner->playArea.height * 0.9f;
            if (e->position.y < despawnY) {
                e->markedForRemoval = true;
            }

            // Flip direction when entity center crosses the border
            float borderY = owner->playArea.y;
            e->anim.dir = (e->position.y > borderY) ? DIR_UP : DIR_DOWN;
            break;
        }

        case ESTATE_DEAD:
            e->markedForRemoval = true;
            break;
    }

    anim_state_update(&e->anim, deltaTime);
}

void entity_draw(const Entity *e) {
    if (!e || !e->alive || !e->sprite) return;
    sprite_draw(e->sprite, &e->anim, e->position, e->spriteScale);
}

