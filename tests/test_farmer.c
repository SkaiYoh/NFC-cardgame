/*
 * Focused unit tests for src/logic/farmer.c return/deposit behavior.
 *
 * Self-contained: stubs the small subset of types/functions farmer.c needs and
 * includes the production .c files directly, following the project's existing
 * assert-based test style.
 */

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/core/config.h"

/* ---- Prevent heavy-header pull-in ---- */
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_FARMER_H
#define NFC_CARDGAME_DEPOSIT_SLOTS_H
#define NFC_CARDGAME_PATHFINDING_H
#define NFC_CARDGAME_BATTLEFIELD_H
#define NFC_CARDGAME_SUSTENANCE_H
#define NFC_CARDGAME_ENTITIES_H
#define NFC_CARDGAME_PLAYER_H

/* ---- Minimal type stubs ---- */
typedef struct { float x; float y; } Vector2;
typedef struct { Vector2 v; } CanonicalPos;

typedef enum { SIDE_BOTTOM, SIDE_TOP } BattleSide;
typedef enum { ENTITY_TROOP, ENTITY_BUILDING, ENTITY_PROJECTILE } EntityType;
typedef enum { ESTATE_IDLE, ESTATE_WALKING, ESTATE_ATTACKING, ESTATE_DEAD } EntityState;
typedef enum { DIR_SIDE, DIR_DOWN, DIR_UP } SpriteDirection;
typedef enum { UNIT_ROLE_COMBAT, UNIT_ROLE_FARMER } UnitRole;
typedef enum {
    FARMER_SEEKING,
    FARMER_WALKING_TO_SUSTENANCE,
    FARMER_GATHERING,
    FARMER_RETURNING,
    FARMER_DEPOSITING
} FarmerState;
typedef enum {
    DEPOSIT_SLOT_NONE = 0,
    DEPOSIT_SLOT_PRIMARY,
    DEPOSIT_SLOT_QUEUE
} DepositSlotKind;
typedef enum {
    SPRITE_TYPE_FARMER = 0,
    SPRITE_TYPE_FARMER_FULL,
} SpriteType;

typedef struct {
    SpriteDirection dir;
    bool flipH;
    bool finished;
} AnimState;

typedef struct {
    float prevNormalized;
    float currNormalized;
    bool finishedThisTick;
    bool loopedThisTick;
} AnimPlaybackEvent;

typedef struct CharacterSprite {
    int unused;
} CharacterSprite;

typedef struct SpriteAtlas {
    int unused;
} SpriteAtlas;

typedef struct {
    Vector2 worldPos;
    int claimedByEntityId;
} DepositSlot;

typedef struct {
    DepositSlot primary[BASE_DEPOSIT_PRIMARY_SLOT_COUNT];
    DepositSlot queue[BASE_DEPOSIT_QUEUE_SLOT_COUNT];
    bool initialized;
} DepositSlotRing;

typedef struct Entity {
    int id;
    EntityType type;
    EntityState state;
    Vector2 position;
    int ownerID;
    float bodyRadius;
    float navRadius;
    BattleSide presentationSide;
    const CharacterSprite *sprite;
    SpriteType spriteType;
    float spriteRotationDegrees;
    AnimState anim;
    FarmerState farmerState;
    int claimedSustenanceNodeId;
    int carriedSustenanceValue;
    float workTimer;
    bool alive;
    bool markedForRemoval;
    int movementTargetId;
    int reservedDepositSlotIndex;
    DepositSlotKind reservedDepositSlotKind;
    UnitRole unitRole;
    DepositSlotRing depositSlots;
} Entity;

typedef struct SustenanceNode {
    int id;
    bool active;
    int claimedByEntityId;
    CanonicalPos worldPos;
    int durability;
    int value;
} SustenanceNode;

typedef struct Battlefield {
    struct {
        SustenanceNode nodes[2][SUSTENANCE_MATCH_COUNT_PER_SIDE];
    } sustenanceField;
} Battlefield;

typedef struct NavFrame {
    bool initialized;
} NavFrame;

#define NAV_MAX_MOBILE_BODY_RADIUS 18
#define NAV_DIST_UNREACHABLE INT_MAX

typedef struct {
    int32_t distance[1];
} NavField;

typedef struct {
    float goalX;
    float goalY;
    float stopRadius;
    int16_t perspectiveSide;
    int32_t carveTargetId;
    float carveCenterX;
    float carveCenterY;
    float carveInnerRadius;
} NavFreeGoalRequest;

typedef struct Player {
    Entity *base;
    int sustenanceBank;
    int sustenanceCollected;
} Player;

typedef struct GameState {
    Player players[2];
    Battlefield battlefield;
    NavFrame nav;
    SpriteAtlas spriteAtlas;
} GameState;

/* ---- External-function stubs used by farmer.c ---- */
static CharacterSprite g_dummySprite;
static bool g_moveArrived = false;
static int g_moveCallCount = 0;
static Vector2 g_lastMoveGoal = { 0.0f, 0.0f };
static float g_lastMoveRadius = 0.0f;
static int g_claimCallCount = 0;
static int g_lastClaimedNodeId = -1;
static int g_animUpdateCalls = 0;
static NavField g_navField;
static bool g_navFieldEnabled = false;
static float g_navUnreachableGoalX = 0.0f;
static float g_navReachableGoalX = 0.0f;
static int32_t g_navReachableCost = 0;

static void reset_move_stub(bool arrived) {
    g_moveArrived = arrived;
    g_moveCallCount = 0;
    g_lastMoveGoal = (Vector2){ 0.0f, 0.0f };
    g_lastMoveRadius = 0.0f;
}

static void reset_sustenance_stub(void) {
    g_claimCallCount = 0;
    g_lastClaimedNodeId = -1;
    g_animUpdateCalls = 0;
    g_navFieldEnabled = false;
    g_navUnreachableGoalX = 0.0f;
    g_navReachableGoalX = 0.0f;
    g_navReachableCost = 0;
    g_navField.distance[0] = NAV_DIST_UNREACHABLE;
}

BattleSide bf_side_for_player(int ownerID) {
    return (ownerID == 0) ? SIDE_BOTTOM : SIDE_TOP;
}

const CharacterSprite *sprite_atlas_get(const SpriteAtlas *atlas, SpriteType type) {
    (void)atlas;
    (void)type;
    return &g_dummySprite;
}

bool pathfind_move_toward_goal(Entity *e, Vector2 goal, float stopRadius,
                               NavFrame *nav, const Battlefield *bf,
                               float deltaTime) {
    (void)nav;
    (void)bf;
    (void)deltaTime;
    g_moveCallCount++;
    g_lastMoveGoal = goal;
    g_lastMoveRadius = stopRadius;
    if (g_moveArrived) {
        e->position = goal;
    }
    return g_moveArrived;
}

void entity_set_state(Entity *e, EntityState newState) {
    e->state = newState;
}

void entity_restart_clip(Entity *e) {
    (void)e;
}

AnimPlaybackEvent anim_state_update(AnimState *anim, float deltaTime) {
    (void)anim;
    (void)deltaTime;
    g_animUpdateCalls++;
    return (AnimPlaybackEvent){ 0 };
}

SustenanceNode *sustenance_find_nearest_available(Battlefield *bf, BattleSide side, Vector2 fromPos) {
    SustenanceNode *best = NULL;
    float bestDistSq = INFINITY;
    for (int i = 0; i < SUSTENANCE_MATCH_COUNT_PER_SIDE; ++i) {
        SustenanceNode *node = &bf->sustenanceField.nodes[side][i];
        if (!node->active || node->claimedByEntityId != -1) continue;
        float dx = node->worldPos.v.x - fromPos.x;
        float dy = node->worldPos.v.y - fromPos.y;
        float distSq = dx * dx + dy * dy;
        if (!best || distSq < bestDistSq) {
            best = node;
            bestDistSq = distSq;
        }
    }
    return best;
}

bool sustenance_claim(Battlefield *bf, int nodeId, int entityId) {
    for (int side = 0; side < 2; ++side) {
        for (int i = 0; i < SUSTENANCE_MATCH_COUNT_PER_SIDE; ++i) {
            SustenanceNode *node = &bf->sustenanceField.nodes[side][i];
            if (node->id != nodeId) continue;
            if (!node->active || node->claimedByEntityId != -1) return false;
            node->claimedByEntityId = entityId;
            g_claimCallCount++;
            g_lastClaimedNodeId = nodeId;
            return true;
        }
    }
    return false;
}

SustenanceNode *sustenance_get_node(Battlefield *bf, int nodeId) {
    for (int side = 0; side < 2; ++side) {
        for (int i = 0; i < SUSTENANCE_MATCH_COUNT_PER_SIDE; ++i) {
            SustenanceNode *node = &bf->sustenanceField.nodes[side][i];
            if (node->id == nodeId) return node;
        }
    }
    return NULL;
}

void sustenance_release_claim(Battlefield *bf, int nodeId, int entityId) {
    SustenanceNode *node = sustenance_get_node(bf, nodeId);
    if (node && node->claimedByEntityId == entityId) {
        node->claimedByEntityId = -1;
    }
}

void sustenance_deplete_and_respawn(Battlefield *bf, int nodeId) {
    (void)bf;
    (void)nodeId;
}

void sustenance_release_claims_for_entity(Battlefield *bf, int entityId) {
    (void)bf;
    (void)entityId;
}

void player_award_sustenance(GameState *gs, int ownerID, int amount) {
    gs->players[ownerID].sustenanceBank += amount;
    gs->players[ownerID].sustenanceCollected += amount;
}

const NavField *nav_get_or_build_free_goal_field(NavFrame *nav,
                                                 const Battlefield *bf,
                                                 const NavFreeGoalRequest *request) {
    (void)nav;
    (void)bf;
    if (!g_navFieldEnabled || !request) return NULL;
    if (fabsf(request->goalX - g_navUnreachableGoalX) < 0.001f) {
        g_navField.distance[0] = NAV_DIST_UNREACHABLE;
    } else if (fabsf(request->goalX - g_navReachableGoalX) < 0.001f) {
        g_navField.distance[0] = g_navReachableCost;
    } else {
        g_navField.distance[0] = NAV_DIST_UNREACHABLE;
    }
    return &g_navField;
}

int32_t nav_cell_index_for_world(float x, float y) {
    (void)x;
    (void)y;
    return 0;
}

/* ---- Production code under test ---- */
#include "../src/logic/deposit_slots.c"
#include "../src/logic/farmer.c"

/* ---- Test helpers ---- */
static Entity make_base(int id, Vector2 position, BattleSide side) {
    Entity base;
    memset(&base, 0, sizeof(base));
    base.id = id;
    base.type = ENTITY_BUILDING;
    base.position = position;
    base.bodyRadius = 16.0f;
    base.navRadius = BASE_NAV_RADIUS;
    base.presentationSide = side;
    base.alive = true;
    deposit_slots_build_for_base(&base);
    return base;
}

static Entity make_farmer(int id, Vector2 position) {
    Entity farmer;
    memset(&farmer, 0, sizeof(farmer));
    farmer.id = id;
    farmer.type = ENTITY_TROOP;
    farmer.state = ESTATE_WALKING;
    farmer.position = position;
    farmer.ownerID = 0;
    farmer.bodyRadius = FARMER_DEFAULT_BODY_RADIUS;
    farmer.spriteType = SPRITE_TYPE_FARMER;
    farmer.farmerState = FARMER_RETURNING;
    farmer.alive = true;
    farmer.movementTargetId = -1;
    farmer.reservedDepositSlotIndex = -1;
    farmer.reservedDepositSlotKind = DEPOSIT_SLOT_NONE;
    farmer.unitRole = UNIT_ROLE_FARMER;
    farmer.anim.dir = DIR_SIDE;
    return farmer;
}

static GameState make_game_state(Entity *base) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.players[0].base = base;
    return gs;
}

static void assert_vec2_near(Vector2 a, Vector2 b) {
    assert(fabsf(a.x - b.x) < 0.001f);
    assert(fabsf(a.y - b.y) < 0.001f);
}

/* ---- Tests ---- */
static void test_open_primary_slots_do_not_queue(void) {
    Entity base = make_base(900, (Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);
    GameState gs = make_game_state(&base);
    Entity farmers[BASE_DEPOSIT_PRIMARY_SLOT_COUNT];
    bool seen[BASE_DEPOSIT_PRIMARY_SLOT_COUNT];
    memset(seen, 0, sizeof(seen));

    reset_move_stub(true);

    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; ++i) {
        farmers[i] = make_farmer(100 + i, (Vector2){ 420.0f + (float)i * 40.0f, 1555.0f });
        farmer_return(&farmers[i], &gs, 1.0f / 60.0f);

        assert(farmers[i].reservedDepositSlotKind == DEPOSIT_SLOT_PRIMARY);
        assert(farmers[i].reservedDepositSlotIndex >= 0);
        assert(farmers[i].reservedDepositSlotIndex < BASE_DEPOSIT_PRIMARY_SLOT_COUNT);
        assert(!seen[farmers[i].reservedDepositSlotIndex]);
        seen[farmers[i].reservedDepositSlotIndex] = true;
        assert(farmers[i].farmerState == FARMER_DEPOSITING);
        assert(farmers[i].state == ESTATE_ATTACKING);
    }
}

static void test_queue_farmer_promotes_immediately_when_primary_releases(void) {
    Entity base = make_base(901, (Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);
    GameState gs = make_game_state(&base);
    Entity farmers[BASE_DEPOSIT_PRIMARY_SLOT_COUNT + 1];

    reset_move_stub(true);

    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT + 1; ++i) {
        farmers[i] = make_farmer(200 + i, (Vector2){ 540.0f, 1560.0f });
        farmer_return(&farmers[i], &gs, 1.0f / 60.0f);
    }

    Entity *queued = &farmers[BASE_DEPOSIT_PRIMARY_SLOT_COUNT];
    assert(queued->reservedDepositSlotKind == DEPOSIT_SLOT_QUEUE);
    assert(queued->state == ESTATE_IDLE);

    deposit_slots_release(&base, DEPOSIT_SLOT_PRIMARY,
                          farmers[0].reservedDepositSlotIndex, farmers[0].id);

    reset_move_stub(true);
    farmer_return(queued, &gs, 1.0f / 60.0f);

    assert(queued->reservedDepositSlotKind == DEPOSIT_SLOT_PRIMARY);
    assert(queued->farmerState == FARMER_DEPOSITING);
    assert(queued->state == ESTATE_ATTACKING);
}

static void test_debug_goal_matches_runtime_goal_for_primary_and_queue(void) {
    Entity base = make_base(902, (Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);
    GameState gs = make_game_state(&base);

    Entity primary = make_farmer(300, (Vector2){ 540.0f, 1700.0f });
    int primaryIdx = -1;
    DepositSlotKind primaryKind = deposit_slots_reserve_for(&base, primary.id,
                                                            primary.position, &primaryIdx);
    assert(primaryKind == DEPOSIT_SLOT_PRIMARY);
    primary.reservedDepositSlotKind = primaryKind;
    primary.reservedDepositSlotIndex = primaryIdx;

    Vector2 debugGoal;
    float debugRadius = 0.0f;
    reset_move_stub(false);
    assert(farmer_debug_nav_goal(&primary, &gs, &debugGoal, &debugRadius));
    farmer_return(&primary, &gs, 1.0f / 60.0f);
    assert(g_moveCallCount == 1);
    assert_vec2_near(g_lastMoveGoal, debugGoal);
    assert(fabsf(g_lastMoveRadius - debugRadius) < 0.001f);
    assert(fabsf(debugRadius - FARMER_DEPOSIT_ARRIVAL_RADIUS) < 0.001f);

    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT - 1; ++i) {
        int idx = -1;
        DepositSlotKind kind = deposit_slots_reserve_for(&base, 400 + i,
                                                         (Vector2){ 540.0f, 1560.0f }, &idx);
        assert(kind == DEPOSIT_SLOT_PRIMARY);
    }

    Entity queued = make_farmer(500, (Vector2){ 540.0f, 1700.0f });
    int queueIdx = -1;
    DepositSlotKind queueKind = deposit_slots_reserve_for(&base, queued.id,
                                                          queued.position, &queueIdx);
    assert(queueKind == DEPOSIT_SLOT_QUEUE);
    queued.reservedDepositSlotKind = queueKind;
    queued.reservedDepositSlotIndex = queueIdx;

    reset_move_stub(false);
    assert(farmer_debug_nav_goal(&queued, &gs, &debugGoal, &debugRadius));
    farmer_return(&queued, &gs, 1.0f / 60.0f);
    assert(g_moveCallCount == 1);
    assert_vec2_near(g_lastMoveGoal, debugGoal);
    assert(fabsf(g_lastMoveRadius - debugRadius) < 0.001f);
    assert(fabsf(debugRadius - FARMER_QUEUE_WAIT_PROXIMITY) < 0.001f);
}

static void test_seek_prefers_nav_reachable_sustenance_over_straight_line_nearest(void) {
    Entity base = make_base(903, (Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);
    GameState gs = make_game_state(&base);
    Entity farmer = make_farmer(600, (Vector2){ 540.0f, 1542.0f });

    reset_sustenance_stub();
    gs.nav.initialized = true;
    farmer.farmerState = FARMER_SEEKING;

    gs.battlefield.sustenanceField.nodes[SIDE_BOTTOM][0] = (SustenanceNode){
        .id = 10,
        .active = true,
        .claimedByEntityId = -1,
        .worldPos = { .v = { 520.0f, 1760.0f } },
        .durability = 1,
        .value = 1,
    };
    gs.battlefield.sustenanceField.nodes[SIDE_BOTTOM][1] = (SustenanceNode){
        .id = 11,
        .active = true,
        .claimedByEntityId = -1,
        .worldPos = { .v = { 720.0f, 1400.0f } },
        .durability = 1,
        .value = 1,
    };

    g_navFieldEnabled = true;
    g_navUnreachableGoalX = gs.battlefield.sustenanceField.nodes[SIDE_BOTTOM][0].worldPos.v.x;
    g_navReachableGoalX = gs.battlefield.sustenanceField.nodes[SIDE_BOTTOM][1].worldPos.v.x;
    g_navReachableCost = 42;

    farmer_seek(&farmer, &gs);

    assert(g_claimCallCount == 1);
    assert(g_lastClaimedNodeId == 11);
    assert(farmer.claimedSustenanceNodeId == 11);
    assert(farmer.farmerState == FARMER_WALKING_TO_SUSTENANCE);
    assert(farmer.state == ESTATE_WALKING);
}

static void test_dead_farmer_returns_without_animating(void) {
    Entity base = make_base(904, (Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);
    GameState gs = make_game_state(&base);
    Entity farmer = make_farmer(601, (Vector2){ 540.0f, 1542.0f });

    reset_sustenance_stub();
    reset_move_stub(false);
    farmer.alive = false;

    farmer_update(&farmer, &gs, 1.0f / 60.0f);

    assert(g_animUpdateCalls == 0);
    assert(g_moveCallCount == 0);
    assert(farmer.markedForRemoval == false);
}

int main(void) {
    printf("Running farmer tests...\n");
    test_open_primary_slots_do_not_queue();
    printf("  PASS: test_open_primary_slots_do_not_queue\n");
    test_queue_farmer_promotes_immediately_when_primary_releases();
    printf("  PASS: test_queue_farmer_promotes_immediately_when_primary_releases\n");
    test_debug_goal_matches_runtime_goal_for_primary_and_queue();
    printf("  PASS: test_debug_goal_matches_runtime_goal_for_primary_and_queue\n");
    test_seek_prefers_nav_reachable_sustenance_over_straight_line_nearest();
    printf("  PASS: test_seek_prefers_nav_reachable_sustenance_over_straight_line_nearest\n");
    test_dead_farmer_returns_without_animating();
    printf("  PASS: test_dead_farmer_returns_without_animating\n");
    printf("All farmer tests passed\n");
    return 0;
}
