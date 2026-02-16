//
// Created by Nathan Davis on 2/16/26.
//

Entity* entity_create(EntityType type, Faction faction, Vector2 pos);
void entity_destroy(Entity *e);
void entity_update(Entity *e, GameState *gs, float deltaTime);
void entity_draw(Entity *e, const Camera2D *camera);