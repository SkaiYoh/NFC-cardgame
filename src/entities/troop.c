//
// Created by Nathan Davis on 2/16/26.
//

typedef struct TroopData {
    const char *name;
    int hp, maxHP;
    int attack;
    float attackSpeed;
    float attackRange;
    float moveSpeed;
    TargetingMode targeting;  // NEAREST, BUILDING, SPECIFIC_TYPE
    const char *targetType;   // "healer", "farmer", etc. (optional)
    Texture2D *sprite;
} TroopData;

Entity* troop_spawn(Player *owner, TroopData *data, Vector2 position);
void troop_update(Entity *troop, GameState *gs, float deltaTime);
void troop_find_target(Entity *troop, GameState *gs);
void troop_move_toward(Entity *troop, Vector2 target, float deltaTime);
void troop_attack(Entity *attacker, Entity *target, float deltaTime);