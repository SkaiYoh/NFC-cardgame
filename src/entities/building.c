//
// Created by Nathan Davis on 2/16/26.
//

#include "building.h"

Entity *building_create_base(Player *owner, Vector2 position) {
    (void)owner;
    (void)position;
    return NULL;
}

void building_take_damage(Entity *building, int damage) {
    (void)building;
    (void)damage;
}
