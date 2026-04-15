/*
 * Unit tests for debug overlay key toggles used by game debug input.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define RAYLIB_H

typedef struct { float x; float y; } Vector2;
typedef struct { float x; float y; float width; float height; } Rectangle;
typedef struct {
    Vector2 offset;
    Vector2 target;
    float rotation;
    float zoom;
} Camera2D;

#define KEY_F2 291
#define KEY_F3 292
#define KEY_F4 293
#define KEY_F5 294
#define KEY_F6 295
#define KEY_F7 296
#define KEY_F8 297
#define KEY_F9 298
#define KEY_F10 299

static Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera) {
    (void)position;
    (void)camera;
    return (Vector2){ 0.0f, 0.0f };
}

#define NFC_CARDGAME_DEBUG_OVERLAY_H

typedef struct Entity {
    int id;
    Vector2 position;
    bool alive;
    bool markedForRemoval;
} Entity;

typedef struct Battlefield {
    Entity *entities[16];
    int entityCount;
} Battlefield;

typedef struct {
    bool attackBars;
    bool targetLines;
    bool eventFlashes;
    bool rangeCirlces;
    bool sustenanceNodes;
    bool sustenancePlacement;
    bool navOverlay;
    bool depositSlots;
    bool crowdShells;
} DebugOverlayFlags;

typedef struct {
    bool hasFocus;
    int focusEntityId;
    Vector2 mouseWorld;
} DebugNavOverlayState;

bool debug_overlay_toggle_key(DebugOverlayFlags *flags, int key);
DebugNavOverlayState debug_overlay_resolve_nav_state(const Battlefield *bf,
                                                     Rectangle battlefieldArea,
                                                     Camera2D camera,
                                                     Vector2 mouseScreen,
                                                     bool useLocalCoords);

#include "../src/rendering/debug_overlay_input.c"

static void test_f8_toggles_nav_overlay_only(void) {
    DebugOverlayFlags flags = {0};
    flags.attackBars = true;
    flags.depositSlots = true;

    assert(debug_overlay_toggle_key(&flags, KEY_F8));
    assert(flags.navOverlay);
    assert(flags.attackBars);
    assert(flags.depositSlots);

    assert(debug_overlay_toggle_key(&flags, KEY_F8));
    assert(!flags.navOverlay);
    assert(flags.attackBars);
    assert(flags.depositSlots);
    printf("  PASS: test_f8_toggles_nav_overlay_only\n");
}

int main(void) {
    printf("Running game debug input tests...\n");
    test_f8_toggles_nav_overlay_only();
    printf("All game debug input tests passed!\n");
    return 0;
}
