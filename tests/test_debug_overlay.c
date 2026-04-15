/*
 * Unit tests for debug nav overlay focus resolution.
 *
 * We include the lightweight debug_overlay_input.c implementation directly and
 * stub just the small raylib/type surface it depends on.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

#ifndef PI_F
#define PI_F 3.14159265f
#endif

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
    float zoom = (camera.zoom == 0.0f) ? 1.0f : camera.zoom;
    float radians = -camera.rotation * PI_F / 180.0f;
    float localX = (position.x - camera.offset.x) / zoom;
    float localY = (position.y - camera.offset.y) / zoom;
    float cosR = cosf(radians);
    float sinR = sinf(radians);
    return (Vector2){
        localX * cosR - localY * sinR + camera.target.x,
        localX * sinR + localY * cosR + camera.target.y
    };
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

static bool approx_eq(float a, float b, float eps) {
    return fabsf(a - b) <= eps;
}

static Vector2 world_to_screen(Vector2 world, Camera2D camera) {
    float zoom = (camera.zoom == 0.0f) ? 1.0f : camera.zoom;
    float radians = camera.rotation * PI_F / 180.0f;
    float dx = world.x - camera.target.x;
    float dy = world.y - camera.target.y;
    float cosR = cosf(radians);
    float sinR = sinf(radians);
    return (Vector2){
        (dx * cosR - dy * sinR) * zoom + camera.offset.x,
        (dx * sinR + dy * cosR) * zoom + camera.offset.y
    };
}

static void test_p1_focus_uses_screen_to_world_directly(void) {
    Battlefield bf = {0};
    Entity a = { .id = 11, .position = { 300.0f, 400.0f }, .alive = true };
    bf.entities[bf.entityCount++] = &a;

    Rectangle area = { 180.0f, 0.0f, 780.0f, 1080.0f };
    Camera2D camera = { .offset = { 0.0f, 0.0f }, .target = { 0.0f, 0.0f }, .zoom = 1.0f };
    DebugNavOverlayState state =
        debug_overlay_resolve_nav_state(&bf, area, camera, a.position, false);

    assert(state.hasFocus);
    assert(state.focusEntityId == a.id);
    assert(approx_eq(state.mouseWorld.x, a.position.x, 0.001f));
    assert(approx_eq(state.mouseWorld.y, a.position.y, 0.001f));
    printf("  PASS: test_p1_focus_uses_screen_to_world_directly\n");
}

static Vector2 p2_world_to_final_screen(Vector2 world, Camera2D camera, Rectangle area) {
    Vector2 rtLocal = world_to_screen(world, camera);
    return (Vector2){ area.x + rtLocal.x, area.y + rtLocal.y };
}

static void test_p2_focus_uses_render_texture_local_coords(void) {
    Battlefield bf = {0};
    Entity a = { .id = 22, .position = { 640.0f, 570.0f }, .alive = true };
    bf.entities[bf.entityCount++] = &a;

    Rectangle area = { 960.0f, 0.0f, 780.0f, 1080.0f };
    Camera2D camera = {
        .offset = { area.width / 2.0f, area.height / 2.0f },
        .target = { 540.0f, 570.0f },
        .rotation = 90.0f,
        .zoom = 1.0f
    };
    Vector2 mouseScreen = p2_world_to_final_screen(a.position, camera, area);
    DebugNavOverlayState state =
        debug_overlay_resolve_nav_state(&bf, area, camera, mouseScreen, true);

    assert(state.hasFocus);
    assert(state.focusEntityId == a.id);
    assert(approx_eq(state.mouseWorld.x, a.position.x, 0.001f));
    assert(approx_eq(state.mouseWorld.y, a.position.y, 0.001f));
    printf("  PASS: test_p2_focus_uses_render_texture_local_coords\n");
}

static void test_p2_mouse_down_moves_world_x_forward(void) {
    Battlefield bf = {0};
    Rectangle area = { 960.0f, 0.0f, 780.0f, 1080.0f };
    Camera2D camera = {
        .offset = { area.width / 2.0f, area.height / 2.0f },
        .target = { 540.0f, 570.0f },
        .rotation = 90.0f,
        .zoom = 1.0f
    };

    DebugNavOverlayState upper =
        debug_overlay_resolve_nav_state(&bf, area, camera,
                                        (Vector2){ area.x + 390.0f, area.y + 440.0f }, true);
    DebugNavOverlayState lower =
        debug_overlay_resolve_nav_state(&bf, area, camera,
                                        (Vector2){ area.x + 390.0f, area.y + 640.0f }, true);

    assert(upper.hasFocus);
    assert(lower.hasFocus);
    assert(lower.mouseWorld.x > upper.mouseWorld.x);
    assert(approx_eq(lower.mouseWorld.y, upper.mouseWorld.y, 0.001f));
    printf("  PASS: test_p2_mouse_down_moves_world_x_forward\n");
}

static void test_mouse_inside_viewport_without_nearby_entity_keeps_empty_focus(void) {
    Battlefield bf = {0};
    Entity a = { .id = 33, .position = { 300.0f, 300.0f }, .alive = true };
    bf.entities[bf.entityCount++] = &a;

    Rectangle area = { 180.0f, 0.0f, 780.0f, 1080.0f };
    Camera2D camera = { .offset = { 0.0f, 0.0f }, .target = { 0.0f, 0.0f }, .zoom = 1.0f };
    Vector2 mouseScreen = { 700.0f, 900.0f };
    DebugNavOverlayState state =
        debug_overlay_resolve_nav_state(&bf, area, camera, mouseScreen, false);

    assert(state.hasFocus);
    assert(state.focusEntityId == -1);
    assert(approx_eq(state.mouseWorld.x, mouseScreen.x, 0.001f));
    assert(approx_eq(state.mouseWorld.y, mouseScreen.y, 0.001f));
    printf("  PASS: test_mouse_inside_viewport_without_nearby_entity_keeps_empty_focus\n");
}

static void test_focus_respects_camera_rotation(void) {
    Battlefield bf = {0};
    Entity a = { .id = 44, .position = { 540.0f, 960.0f }, .alive = true };
    bf.entities[bf.entityCount++] = &a;

    Rectangle area = { 180.0f, 0.0f, 780.0f, 1080.0f };
    Camera2D camera = {
        .offset = { 570.0f, 540.0f },
        .target = { 540.0f, 960.0f },
        .rotation = 90.0f,
        .zoom = 1.0f
    };
    Vector2 mouseScreen = world_to_screen(a.position, camera);
    DebugNavOverlayState state =
        debug_overlay_resolve_nav_state(&bf, area, camera, mouseScreen, false);

    assert(state.hasFocus);
    assert(state.focusEntityId == a.id);
    assert(approx_eq(state.mouseWorld.x, a.position.x, 0.001f));
    assert(approx_eq(state.mouseWorld.y, a.position.y, 0.001f));
    printf("  PASS: test_focus_respects_camera_rotation\n");
}

static void test_viewport_seam_belongs_only_to_p2(void) {
    Battlefield bf = {0};
    Rectangle p1Area = { 180.0f, 0.0f, 780.0f, 1080.0f };
    Rectangle p2Area = { 960.0f, 0.0f, 780.0f, 1080.0f };
    Camera2D camera = { .zoom = 1.0f };
    Vector2 seamPoint = { 960.0f, 300.0f };

    DebugNavOverlayState p1 =
        debug_overlay_resolve_nav_state(&bf, p1Area, camera, seamPoint, false);
    DebugNavOverlayState p2 =
        debug_overlay_resolve_nav_state(&bf, p2Area, camera, seamPoint, false);

    assert(!p1.hasFocus);
    assert(p2.hasFocus);
    printf("  PASS: test_viewport_seam_belongs_only_to_p2\n");
}

int main(void) {
    printf("Running debug_overlay focus tests...\n");
    test_p1_focus_uses_screen_to_world_directly();
    test_p2_focus_uses_render_texture_local_coords();
    test_p2_mouse_down_moves_world_x_forward();
    test_mouse_inside_viewport_without_nearby_entity_keeps_empty_focus();
    test_focus_respects_camera_rotation();
    test_viewport_seam_belongs_only_to_p2();
    printf("All debug_overlay tests passed!\n");
    return 0;
}
