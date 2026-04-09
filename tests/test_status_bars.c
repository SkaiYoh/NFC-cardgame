/*
 * Unit tests for src/rendering/status_bars.c
 *
 * Self-contained: includes production code directly with minimal Raylib/type
 * stubs so troop/base bar draw behavior can be validated without the full app.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

/* ---- Prevent production headers from pulling in heavy dependencies ---- */
#define RAYLIB_H
#define NFC_CARDGAME_CONFIG_H
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_STATUS_BARS_H
#define NFC_CARDGAME_SPRITE_RENDERER_H

/* ---- Raylib stubs ---- */
typedef struct { float x; float y; } Vector2;
typedef struct { float x; float y; float width; float height; } Rectangle;
typedef struct {
    unsigned int id;
    int width;
    int height;
    int mipmaps;
    int format;
} Texture2D;
typedef struct { unsigned char r, g, b, a; } Color;
typedef struct {
    Vector2 offset;
    Vector2 target;
    float rotation;
    float zoom;
} Camera2D;

#define WHITE (Color){255, 255, 255, 255}
#define TEXTURE_FILTER_POINT 0

static int g_drawCalls = 0;
static Rectangle g_drawSrcs[8];
static Rectangle g_drawDsts[8];

static Texture2D LoadTexture(const char *fileName) {
    (void)fileName;
    return (Texture2D){ .id = 1, .width = 2048, .height = 64, .mipmaps = 1, .format = 7 };
}

static void UnloadTexture(Texture2D texture) {
    (void)texture;
}

static void SetTextureFilter(Texture2D texture, int filter) {
    (void)texture;
    (void)filter;
}

static void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                           Vector2 origin, float rotation, Color tint) {
    (void)texture;
    (void)origin;
    (void)rotation;
    (void)tint;

    if (g_drawCalls < (int)(sizeof(g_drawSrcs) / sizeof(g_drawSrcs[0]))) {
        g_drawSrcs[g_drawCalls] = source;
        g_drawDsts[g_drawCalls] = dest;
    }
    g_drawCalls++;
}

static Vector2 GetWorldToScreen2D(Vector2 position, Camera2D camera) {
    (void)camera;
    return position;
}

/* ---- Config stubs ---- */
#define STATUS_BARS_PATH "src/assets/environment/Objects/health_energy_bars.png"
#define PI_F 3.14159265f
#define MAX_ENTITIES 64

/* ---- Minimal sprite/type stubs ---- */
typedef enum {
    ANIM_IDLE,
    ANIM_RUN,
    ANIM_WALK,
    ANIM_HURT,
    ANIM_DEATH,
    ANIM_ATTACK,
    ANIM_COUNT
} AnimationType;

typedef enum {
    DIR_SIDE,
    DIR_DOWN,
    DIR_UP,
    DIR_COUNT
} SpriteDirection;

typedef struct {
    Texture2D texture;
    int frameWidth;
    int frameHeight;
    int frameCount;
    Rectangle *visibleBounds;
} SpriteSheet;

typedef struct {
    SpriteSheet anims[ANIM_COUNT];
} CharacterSprite;

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

static const SpriteSheet *sprite_sheet_get(const CharacterSprite *cs, AnimationType anim) {
    (void)cs;
    (void)anim;
    return NULL;
}

static Rectangle sprite_visible_bounds(const CharacterSprite *cs, const AnimState *state,
                                       Vector2 pos, float scale, float rotationDegrees) {
    (void)cs;
    (void)state;
    (void)pos;
    (void)scale;
    (void)rotationDegrees;
    return (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
}

/* ---- Minimal game type stubs ---- */
typedef enum { ENTITY_TROOP, ENTITY_BUILDING, ENTITY_PROJECTILE } EntityType;
typedef enum { FACTION_PLAYER1, FACTION_PLAYER2 } Faction;
typedef enum { ESTATE_IDLE, ESTATE_WALKING, ESTATE_ATTACKING, ESTATE_DEAD } EntityState;

typedef struct Entity {
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
    int targeting;
    const char *targetType;
    AnimState anim;
    const CharacterSprite *sprite;
    int spriteType;
    float spriteScale;
    float spriteRotationDegrees;
    int presentationSide;
    int ownerID;
    int lane;
    int waypointIndex;
    float hitFlashTimer;
    int unitRole;
    int farmerState;
    int claimedSustenanceNodeId;
    int carriedSustenanceValue;
    float workTimer;
    bool alive;
    bool markedForRemoval;
} Entity;

typedef struct Player {
    int id;
    int side;
    Rectangle screenArea;
    Camera2D camera;
    float cameraRotation;
    float energy;
    float maxEnergy;
    float energyRegenRate;
    Entity *base;
    int sustenanceCollected;
} Player;

typedef struct Battlefield {
    Entity *entities[MAX_ENTITIES];
    int entityCount;
} Battlefield;

typedef struct GameState {
    Player players[2];
    Battlefield battlefield;
    Texture2D statusBarsTexture;
} GameState;

/* ---- Include production code ---- */
#include "../src/rendering/status_bars.c"

/* ---- Test helpers ---- */
static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(fn) do { \
    reset_draw_state(); \
    printf("  "); \
    fn(); \
    tests_run++; \
    tests_passed++; \
    printf("PASS: %s\n", #fn); \
} while (0)

static bool approx_eq(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

static void reset_draw_state(void) {
    g_drawCalls = 0;
    for (int i = 0; i < (int)(sizeof(g_drawSrcs) / sizeof(g_drawSrcs[0])); i++) {
        g_drawSrcs[i] = (Rectangle){0};
        g_drawDsts[i] = (Rectangle){0};
    }
}

static Entity make_troop(int hp, int maxHP) {
    Entity troop = {0};
    troop.type = ENTITY_TROOP;
    troop.hp = hp;
    troop.maxHP = maxHP;
    troop.alive = true;
    troop.position = (Vector2){100.0f, 200.0f};
    return troop;
}

static Entity make_base(int hp, int maxHP) {
    Entity base = {0};
    base.type = ENTITY_BUILDING;
    base.hp = hp;
    base.maxHP = maxHP;
    base.alive = true;
    base.position = (Vector2){400.0f, 500.0f};
    return base;
}

static GameState make_game_state(void) {
    GameState gs = {0};
    gs.statusBarsTexture = (Texture2D){ .id = 1, .width = 2048, .height = 64, .mipmaps = 1, .format = 7 };
    gs.players[0].maxEnergy = 10.0f;
    gs.players[1].maxEnergy = 10.0f;
    return gs;
}

/* ---- Tests ---- */
static void test_full_health_troop_frame_hidden(void) {
    Entity troop = make_troop(100, 100);

    assert(troop_health_frame(&troop) == -1);
}

static void test_damaged_troop_frame_visible(void) {
    Entity troop = make_troop(75, 100);
    int frame = troop_health_frame(&troop);

    assert(frame >= 0);
    assert(frame < STATUS_BAR_TROOP_FRAMES);
}

static void test_status_bars_draw_skips_undamaged_troops(void) {
    GameState gs = make_game_state();
    Entity troop = make_troop(100, 100);
    Camera2D camera = {0};

    gs.battlefield.entities[0] = &troop;
    gs.battlefield.entityCount = 1;

    status_bars_draw_screen(&gs, camera, 90.0f);

    assert(g_drawCalls == 0);
}

static void test_status_bars_draws_damaged_troop_bar(void) {
    GameState gs = make_game_state();
    Entity troop = make_troop(80, 100);
    Camera2D camera = {0};

    gs.battlefield.entities[0] = &troop;
    gs.battlefield.entityCount = 1;

    status_bars_draw_screen(&gs, camera, 90.0f);

    assert(g_drawCalls == 1);
    assert(approx_eq(g_drawSrcs[0].y, 0.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[0].width, 42.0f, 0.001f));
}

static void test_status_bars_draws_base_bars_unchanged(void) {
    GameState gs = make_game_state();
    Entity base = make_base(4500, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 7.0f;
    gs.players[0].maxEnergy = 10.0f;

    status_bars_draw_screen(&gs, camera, 90.0f);

    assert(g_drawCalls == 2);
    assert(approx_eq(g_drawSrcs[0].y, 16.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[1].y, 32.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[0].width, 138.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[1].width, 138.0f, 0.001f));
}

int main(void) {
    printf("Running status bar tests...\n");

    RUN_TEST(test_full_health_troop_frame_hidden);
    RUN_TEST(test_damaged_troop_frame_visible);
    RUN_TEST(test_status_bars_draw_skips_undamaged_troops);
    RUN_TEST(test_status_bars_draws_damaged_troop_bar);
    RUN_TEST(test_status_bars_draws_base_bars_unchanged);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return 0;
}
