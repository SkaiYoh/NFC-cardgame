/*
 * Unit tests for src/entities/entities.c attack-state behavior.
 *
 * Self-contained: redefines minimal type stubs and includes entities.c
 * directly to avoid the heavy types.h include chain.
 *
 * Focus: healer stale-target cancel/retarget behavior in ESTATE_ATTACKING.
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Prevent entities.c's includes from pulling in heavy headers ---- */
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_ENTITIES_H
#define NFC_CARDGAME_ENTITY_ANIMATION_H
#define NFC_CARDGAME_BATTLEFIELD_H
#define NFC_CARDGAME_DEBUG_EVENTS_H
#define NFC_CARDGAME_PATHFINDING_H
#define NFC_CARDGAME_COMBAT_H
#define NFC_CARDGAME_FARMER_H

/* ---- Minimal type stubs ---- */
typedef struct { float x; float y; } Vector2;
typedef struct { float x; float y; float width; float height; } Rectangle;

typedef enum { ENTITY_TROOP, ENTITY_BUILDING, ENTITY_PROJECTILE } EntityType;
typedef enum { FACTION_PLAYER1, FACTION_PLAYER2 } Faction;
typedef enum { ESTATE_IDLE, ESTATE_WALKING, ESTATE_ATTACKING, ESTATE_DEAD } EntityState;
typedef enum { TARGET_NEAREST, TARGET_BUILDING, TARGET_SPECIFIC_TYPE } TargetingMode;
typedef enum { UNIT_ROLE_COMBAT, UNIT_ROLE_FARMER } UnitRole;
typedef enum {
    FARMER_SEEKING,
    FARMER_WALKING_TO_SUSTENANCE,
    FARMER_GATHERING,
    FARMER_RETURNING,
    FARMER_DEPOSITING
} FarmerState;
typedef enum { SIDE_BOTTOM, SIDE_TOP } BattleSide;
typedef enum {
    ANIM_IDLE,
    ANIM_RUN,
    ANIM_WALK,
    ANIM_HURT,
    ANIM_DEATH,
    ANIM_ATTACK,
    ANIM_COUNT
} AnimationType;
typedef enum { DIR_SIDE, DIR_DOWN, DIR_UP, DIR_COUNT } SpriteDirection;
typedef enum {
    SPRITE_TYPE_KNIGHT,
    SPRITE_TYPE_HEALER,
    SPRITE_TYPE_ASSASSIN,
    SPRITE_TYPE_BRUTE,
    SPRITE_TYPE_FARMER,
    SPRITE_TYPE_BASE,
    SPRITE_TYPE_COUNT
} SpriteType;
typedef enum {
    ANIM_PLAY_LOOP,
    ANIM_PLAY_ONCE
} AnimPlayMode;

typedef struct {
    AnimationType anim;
    SpriteDirection dir;
    float elapsed;
    float cycleDuration;
    float normalizedTime;
    bool oneShot;
    bool finished;
    bool flipH;
} AnimState;

typedef struct {
    float prevNormalized;
    float currNormalized;
    bool finishedThisTick;
    bool loopedThisTick;
} AnimPlaybackEvent;

typedef struct {
    AnimationType anim;
    AnimPlayMode mode;
    float cycleSeconds;
    float hitNormalized;
    bool lockFacing;
    bool removeOnFinish;
} EntityAnimSpec;

#define WALK_PIXELS_PER_CYCLE 64.0f

typedef struct { int dummy; } CharacterSprite;

typedef struct Entity Entity;
typedef struct GameState GameState;

struct Entity {
    int id;
    EntityType type;
    Faction faction;
    EntityState state;
    Vector2 position;
    float moveSpeed;
    int hp, maxHP;
    int attack;
    float attackSpeed;
    float attackRange;
    float attackCooldown;
    int attackTargetId;
    TargetingMode targeting;
    const char *targetType;
    AnimState anim;
    const CharacterSprite *sprite;
    SpriteType spriteType;
    float spriteScale;
    float spriteRotationDegrees;
    BattleSide presentationSide;
    int ownerID;
    int lane;
    int waypointIndex;
    float hitFlashTimer;
    UnitRole unitRole;
    FarmerState farmerState;
    int claimedSustenanceNodeId;
    int carriedSustenanceValue;
    float workTimer;
    bool alive;
    bool markedForRemoval;
    int healAmount;
};

typedef struct Battlefield {
    Entity *entities[16];
    int entityCount;
} Battlefield;

struct GameState {
    Battlefield battlefield;
};

/* ---- Test globals ---- */
static Entity *g_findTargetResult = NULL;
static int g_applyHitCalls = 0;
static Entity *g_lastApplyHitAttacker = NULL;
static Entity *g_lastApplyHitTarget = NULL;

/* ---- Stubs required by entities.c ---- */
enum { DEBUG_EVT_STATE_CHANGE = 0, DEBUG_EVT_HIT = 1, DEBUG_EVT_DEATH_FINISH = 2 };

void debug_event_emit_xy(float x, float y, int type) {
    (void)x;
    (void)y;
    (void)type;
}

void farmer_update(Entity *e, GameState *gs, float deltaTime) {
    (void)e;
    (void)gs;
    (void)deltaTime;
}

static float distance_between(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

bool combat_in_range(const Entity *a, const Entity *b, const GameState *gs) {
    (void)gs;
    if (!a || !b) return false;
    return distance_between(a->position, b->position) <= a->attackRange;
}

Entity *combat_find_target(Entity *attacker, GameState *gs) {
    (void)attacker;
    (void)gs;
    return g_findTargetResult;
}

bool combat_can_heal_target(const Entity *attacker, const Entity *target) {
    if (!attacker || !target) return false;
    if (attacker->healAmount <= 0) return false;
    if (target == attacker) return false;
    if (target->ownerID != attacker->ownerID) return false;
    if (target->type != ENTITY_TROOP) return false;
    if (!target->alive || target->markedForRemoval) return false;
    if (target->hp >= target->maxHP) return false;
    return true;
}

void combat_apply_hit(Entity *attacker, Entity *target, GameState *gs) {
    (void)gs;
    g_applyHitCalls++;
    g_lastApplyHitAttacker = attacker;
    g_lastApplyHitTarget = target;
}

void pathfind_step_entity(Entity *e, const Battlefield *bf, float deltaTime) {
    (void)e;
    (void)bf;
    (void)deltaTime;
}

void pathfind_commit_presentation(Entity *e, const Battlefield *bf) {
    (void)e;
    (void)bf;
}

void pathfind_apply_direction_for_side(AnimState *anim, Vector2 diff, BattleSide side) {
    (void)side;
    if (fabsf(diff.x) >= fabsf(diff.y)) {
        anim->dir = DIR_SIDE;
    } else {
        anim->dir = (diff.y >= 0.0f) ? DIR_DOWN : DIR_UP;
    }
}

float pathfind_sprite_rotation_for_side(SpriteDirection dir, BattleSide side) {
    (void)dir;
    (void)side;
    return 0.0f;
}

void pathfind_update_walk_facing(Entity *e, const Battlefield *bf) {
    (void)e;
    (void)bf;
}

Entity *bf_find_entity(const Battlefield *bf, int id) {
    if (!bf) return NULL;
    for (int i = 0; i < bf->entityCount; i++) {
        Entity *candidate = bf->entities[i];
        if (candidate && candidate->id == id) return candidate;
    }
    return NULL;
}

const EntityAnimSpec *anim_spec_get(SpriteType spriteType, AnimationType animType) {
    (void)spriteType;
    static const EntityAnimSpec s_idle = { ANIM_IDLE, ANIM_PLAY_LOOP, 0.5f, -1.0f, false, false };
    static const EntityAnimSpec s_walk = { ANIM_WALK, ANIM_PLAY_LOOP, 0.8f, -1.0f, false, false };
    static const EntityAnimSpec s_attack = { ANIM_ATTACK, ANIM_PLAY_ONCE, 1.0f, 0.5f, true, false };
    static const EntityAnimSpec s_death = { ANIM_DEATH, ANIM_PLAY_ONCE, 0.75f, -1.0f, false, true };

    switch (animType) {
        case ANIM_ATTACK: return &s_attack;
        case ANIM_WALK: return &s_walk;
        case ANIM_DEATH: return &s_death;
        case ANIM_IDLE:
        case ANIM_RUN:
        case ANIM_HURT:
        default:
            return &s_idle;
    }
}

float anim_walk_cycle_seconds(float moveSpeed, float pixelsPerCycle) {
    (void)moveSpeed;
    (void)pixelsPerCycle;
    return 1.0f;
}

float anim_attack_cycle_seconds(float attackSpeed) {
    if (attackSpeed <= 0.0f) return 1.0f;
    return 1.0f / attackSpeed;
}

void anim_state_init(AnimState *state, AnimationType anim, SpriteDirection dir,
                     float cycleDuration, bool oneShot) {
    state->anim = anim;
    state->dir = dir;
    state->elapsed = 0.0f;
    state->cycleDuration = cycleDuration;
    state->normalizedTime = 0.0f;
    state->oneShot = oneShot;
    state->finished = false;
    state->flipH = false;
}

AnimPlaybackEvent anim_state_update(AnimState *state, float dt) {
    AnimPlaybackEvent evt = {0};
    if (!state) return evt;

    evt.prevNormalized = state->normalizedTime;
    if (state->finished) {
        evt.currNormalized = state->normalizedTime;
        return evt;
    }

    if (state->cycleDuration <= 0.0f) {
        if (state->oneShot) {
            state->finished = true;
            state->normalizedTime = 1.0f;
            evt.finishedThisTick = true;
        }
        evt.currNormalized = state->normalizedTime;
        return evt;
    }

    state->elapsed += dt;
    if (state->oneShot) {
        if (state->elapsed >= state->cycleDuration) {
            state->elapsed = state->cycleDuration;
            state->normalizedTime = 1.0f;
            state->finished = true;
            evt.finishedThisTick = true;
        } else {
            state->normalizedTime = state->elapsed / state->cycleDuration;
        }
    } else {
        while (state->elapsed >= state->cycleDuration) {
            state->elapsed -= state->cycleDuration;
            evt.loopedThisTick = true;
        }
        state->normalizedTime = state->elapsed / state->cycleDuration;
    }

    evt.currNormalized = state->normalizedTime;
    return evt;
}

void sprite_draw(const CharacterSprite *cs, const AnimState *state,
                 Vector2 pos, float scale, float rotationDegrees) {
    (void)cs;
    (void)state;
    (void)pos;
    (void)scale;
    (void)rotationDegrees;
}

void entity_set_state(Entity *e, EntityState newState);
void entity_restart_clip(Entity *e);

/* ---- Include production code under test ---- */
#include "../src/entities/entities.c"

/* ---- Test helpers ---- */
static int testsRun = 0;
static int testsPassed = 0;

#define RUN_TEST(fn) do { \
    printf("  "); \
    fn(); \
    testsRun++; \
    testsPassed++; \
    printf("PASS: %s\n", #fn); \
} while (0)

static void reset_globals(void) {
    g_findTargetResult = NULL;
    g_applyHitCalls = 0;
    g_lastApplyHitAttacker = NULL;
    g_lastApplyHitTarget = NULL;
}

static void battlefield_add(Battlefield *bf, Entity *entity) {
    bf->entities[bf->entityCount++] = entity;
}

static GameState make_game_state(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    return gs;
}

static Entity make_entity(int id, int ownerID, EntityType type, Vector2 pos) {
    Entity e;
    memset(&e, 0, sizeof(e));
    e.id = id;
    e.type = type;
    e.faction = (ownerID == 0) ? FACTION_PLAYER1 : FACTION_PLAYER2;
    e.state = ESTATE_WALKING;
    e.position = pos;
    e.moveSpeed = 60.0f;
    e.hp = 100;
    e.maxHP = 100;
    e.attack = 5;
    e.attackSpeed = 1.0f;
    e.attackRange = 80.0f;
    e.attackTargetId = -1;
    e.targeting = TARGET_NEAREST;
    e.presentationSide = SIDE_BOTTOM;
    e.ownerID = ownerID;
    e.lane = 1;
    e.waypointIndex = 1;
    e.unitRole = UNIT_ROLE_COMBAT;
    e.alive = true;
    e.markedForRemoval = false;
    e.spriteType = SPRITE_TYPE_KNIGHT;
    anim_state_init(&e.anim, ANIM_WALK, DIR_SIDE, 1.0f, false);
    return e;
}

static Entity make_healer(int id, Vector2 pos) {
    Entity healer = make_entity(id, 0, ENTITY_TROOP, pos);
    healer.state = ESTATE_ATTACKING;
    healer.healAmount = 8;
    healer.spriteType = SPRITE_TYPE_HEALER;
    anim_state_init(&healer.anim, ANIM_ATTACK, DIR_SIDE, 1.0f, true);
    healer.anim.elapsed = 0.49f;
    healer.anim.normalizedTime = 0.49f;
    return healer;
}

static void test_healer_cancels_stale_heal_and_retargets_enemy(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity healer = make_healer(1, (Vector2){0.0f, 0.0f});
    Entity ally = make_entity(2, 0, ENTITY_TROOP, (Vector2){10.0f, 0.0f});
    Entity enemy = make_entity(3, 1, ENTITY_TROOP, (Vector2){-10.0f, 0.0f});
    healer.attackTargetId = ally.id;

    ally.hp = ally.maxHP; /* stale lock: no longer injured */

    battlefield_add(&gs.battlefield, &ally);
    battlefield_add(&gs.battlefield, &enemy);
    g_findTargetResult = &enemy;

    entity_update(&healer, &gs, 0.05f);

    assert(healer.state == ESTATE_ATTACKING);
    assert(healer.attackTargetId == enemy.id);
    assert(g_applyHitCalls == 0);
    assert(healer.anim.normalizedTime < 0.10f);
}

static void test_healer_cancels_stale_heal_and_walks_without_replacement(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity healer = make_healer(1, (Vector2){0.0f, 0.0f});
    Entity ally = make_entity(2, 0, ENTITY_TROOP, (Vector2){10.0f, 0.0f});
    healer.attackTargetId = ally.id;
    ally.hp = ally.maxHP;

    battlefield_add(&gs.battlefield, &ally);

    entity_update(&healer, &gs, 0.05f);

    assert(healer.state == ESTATE_WALKING);
    assert(healer.attackTargetId == -1);
    assert(g_applyHitCalls == 0);
}

static void test_non_healer_enemy_hit_flow_unchanged(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){10.0f, 0.0f});
    attacker.state = ESTATE_ATTACKING;
    attacker.attackTargetId = enemy.id;
    anim_state_init(&attacker.anim, ANIM_ATTACK, DIR_SIDE, 1.0f, true);
    attacker.anim.elapsed = 0.49f;
    attacker.anim.normalizedTime = 0.49f;

    battlefield_add(&gs.battlefield, &enemy);

    entity_update(&attacker, &gs, 0.05f);

    assert(attacker.state == ESTATE_ATTACKING);
    assert(attacker.attackTargetId == enemy.id);
    assert(g_applyHitCalls == 1);
    assert(g_lastApplyHitAttacker == &attacker);
    assert(g_lastApplyHitTarget == &enemy);
}

int main(void) {
    printf("Running entities tests...\n");

    RUN_TEST(test_healer_cancels_stale_heal_and_retargets_enemy);
    RUN_TEST(test_healer_cancels_stale_heal_and_walks_without_replacement);
    RUN_TEST(test_non_healer_enemy_hit_flow_unchanged);

    printf("\nAll %d tests passed!\n", testsPassed);
    return 0;
}
