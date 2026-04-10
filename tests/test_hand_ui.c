/*
 * Unit tests for src/rendering/hand_ui.c
 *
 * Scope: hand layout math plus occupancy-based draw behavior for hand_ui.c.
 * We stub raylib and the game type headers so the test compiles with only
 * -lm, matching the header-guard-override pattern used by test_battlefield.c
 * and test_spawn_fx.c.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

/* ---- Raylib stubs ---- */
#define RAYLIB_H

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

#define WHITE (Color){255, 255, 255, 255}
#define TEXTURE_FILTER_POINT 0

/* Stubs for the raylib calls made by the non-tested parts of hand_ui.c.
 * hand_ui_card_center_for_index does not touch any of these, but the other
 * functions in the same translation unit do, so the linker needs them. */
static Texture2D LoadTexture(const char *fileName) {
    (void)fileName;
    return (Texture2D){ .id = 1, .width = 128, .height = 160,
                        .mipmaps = 1, .format = 0 };
}
static void UnloadTexture(Texture2D t) { (void)t; }
static void SetTextureFilter(Texture2D t, int f) { (void)t; (void)f; }

/* ---- Config stub: only the constants hand_ui.c depends on ---- */
#define NFC_CARDGAME_CONFIG_H
#define HAND_UI_DEPTH_PX               180
#define HAND_MAX_CARDS                 8
#define HAND_CARD_WIDTH                128
#define HAND_CARD_HEIGHT               160
#define HAND_CARD_GAP                  4
#define HAND_CARD_PLACEHOLDER_PATH     "src/assets/cards/uvulite_card.png"

static int g_draw_texture_calls = 0;
static Rectangle g_drawn_dst[HAND_MAX_CARDS];
static float g_drawn_rotation[HAND_MAX_CARDS];

static void DrawRectangleRec(Rectangle r, Color c) { (void)r; (void)c; }
static void DrawTexturePro(Texture2D t, Rectangle src, Rectangle dst,
                           Vector2 origin, float rotation, Color tint) {
    (void)t; (void)src; (void)dst; (void)origin; (void)rotation; (void)tint;
    if (g_draw_texture_calls < HAND_MAX_CARDS) {
        g_drawn_dst[g_draw_texture_calls] = dst;
        g_drawn_rotation[g_draw_texture_calls] = rotation;
    }
    g_draw_texture_calls++;
}

/* ---- Skip the heavy types.h chain ---- */
#define NFC_CARDGAME_TYPES_H

typedef enum { SIDE_BOTTOM = 0, SIDE_TOP = 1 } BattleSide;

typedef struct Player {
    int id;
    BattleSide side;
    Rectangle screenArea;
    Rectangle battlefieldArea;
    Rectangle handArea;
    void *handCards[HAND_MAX_CARDS];
} Player;

/* ---- Skip hand_ui.h include chain by providing the declarations ourselves ---- */
#define NFC_CARDGAME_HAND_UI_H
Texture2D hand_ui_load_placeholder(void);
void hand_ui_unload_placeholder(Texture2D texture);
void hand_ui_draw(const Player *p, Texture2D placeholder);
Vector2 hand_ui_card_center_for_index(Rectangle handArea, int visibleCardCount, int visibleIndex);

/* ---- Production code under test ---- */
#include "../src/rendering/hand_ui.c"

/* ---- Test helpers ---- */

static bool approx_eq(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

static void reset_draw_capture(void) {
    g_draw_texture_calls = 0;
    for (int i = 0; i < HAND_MAX_CARDS; i++) {
        g_drawn_dst[i] = (Rectangle){0};
        g_drawn_rotation[i] = 0.0f;
    }
}

/* ---- Test: center X matches handArea midline for common hand sizes ---- */
static void test_center_x_matches_handarea_midline(void) {
    Rectangle p1Hand = { 0.0f, 0.0f, 180.0f, 1080.0f };
    Rectangle p2Hand = { 1740.0f, 0.0f, 180.0f, 1080.0f };

    for (int count = 1; count <= 3; count++) {
        for (int i = 0; i < count; i++) {
            Vector2 p1 = hand_ui_card_center_for_index(p1Hand, count, i);
            Vector2 p2 = hand_ui_card_center_for_index(p2Hand, count, i);
            assert(approx_eq(p1.x, p1Hand.x + p1Hand.width * 0.5f, 0.01f));
            assert(approx_eq(p2.x, p2Hand.x + p2Hand.width * 0.5f, 0.01f));
        }
    }
    printf("  PASS: test_center_x_matches_handarea_midline\n");
}

/* ---- Test: visible cards remain ordered top-to-bottom ---- */
static void test_cards_monotonic_y(void) {
    Rectangle hand = { 0.0f, 0.0f, 180.0f, 1080.0f };
    for (int i = 1; i < 3; i++) {
        Vector2 prev = hand_ui_card_center_for_index(hand, 3, i - 1);
        Vector2 curr = hand_ui_card_center_for_index(hand, 3, i);
        assert(curr.y > prev.y);
    }
    printf("  PASS: test_cards_monotonic_y\n");
}

/* ---- Test: centers are evenly spaced by stride ---- */
static void test_even_stride(void) {
    Rectangle hand = { 0.0f, 0.0f, 180.0f, 1080.0f };
    const float stride = (float)HAND_CARD_WIDTH + (float)HAND_CARD_GAP;
    for (int i = 1; i < 3; i++) {
        Vector2 prev = hand_ui_card_center_for_index(hand, 3, i - 1);
        Vector2 curr = hand_ui_card_center_for_index(hand, 3, i);
        assert(approx_eq(curr.y - prev.y, stride, 0.01f));
    }
    printf("  PASS: test_even_stride\n");
}

/* ---- Test: one-card run is vertically centered inside handArea ---- */
static void test_one_card_centered_in_handarea(void) {
    Rectangle hand = { 0.0f, 0.0f, 180.0f, 1080.0f };
    Vector2 only = hand_ui_card_center_for_index(hand, 1, 0);

    assert(approx_eq(only.y, hand.y + hand.height * 0.5f, 0.01f));
    printf("  PASS: test_one_card_centered_in_handarea\n");
}

/* ---- Test: three-card run is vertically centered inside handArea ---- */
static void test_three_card_run_centered_in_handarea(void) {
    Rectangle hand = { 0.0f, 0.0f, 180.0f, 1080.0f };

    Vector2 first = hand_ui_card_center_for_index(hand, 3, 0);
    Vector2 last  = hand_ui_card_center_for_index(hand, 3, 2);

    const float halfCardWidth = (float)HAND_CARD_WIDTH * 0.5f;
    const float topEdge = first.y - halfCardWidth;
    const float bottomEdge = last.y + halfCardWidth;
    const float topSlack = topEdge - hand.y;
    const float bottomSlack = (hand.y + hand.height) - bottomEdge;

    assert(approx_eq(topSlack, bottomSlack, 0.01f));
    assert(topSlack > 0.0f);

    printf("  PASS: test_three_card_run_centered_in_handarea\n");
}

/* ---- Test: expected absolute values for P1 {0,0,180,1080} ---- */
static void test_expected_absolute_positions_p1(void) {
    Rectangle p1Hand = { 0.0f, 0.0f, 180.0f, 1080.0f };

    Vector2 one = hand_ui_card_center_for_index(p1Hand, 1, 0);
    assert(approx_eq(one.x, 90.0f, 0.01f));
    assert(approx_eq(one.y, 540.0f, 0.01f));

    Vector2 three0 = hand_ui_card_center_for_index(p1Hand, 3, 0);
    Vector2 three1 = hand_ui_card_center_for_index(p1Hand, 3, 1);
    Vector2 three2 = hand_ui_card_center_for_index(p1Hand, 3, 2);
    assert(approx_eq(three0.y, 408.0f, 0.01f));
    assert(approx_eq(three1.y, 540.0f, 0.01f));
    assert(approx_eq(three2.y, 672.0f, 0.01f));

    printf("  PASS: test_expected_absolute_positions_p1\n");
}

/* ---- Test: empty hand emits no draw calls ---- */
static void test_empty_hand_draws_nothing(void) {
    Player p = {0};
    p.side = SIDE_BOTTOM;
    p.handArea = (Rectangle){ 0.0f, 0.0f, 180.0f, 1080.0f };
    Texture2D placeholder = { .id = 1, .width = 128, .height = 160, .mipmaps = 1, .format = 0 };

    reset_draw_capture();
    hand_ui_draw(&p, placeholder);
    assert(g_draw_texture_calls == 0);

    printf("  PASS: test_empty_hand_draws_nothing\n");
}

/* ---- Test: sparse hand compacts visible cards with no gaps ---- */
static void test_sparse_hand_compacts_draw_positions(void) {
    Player p = {0};
    int cardA = 1;
    int cardB = 2;
    Texture2D placeholder = { .id = 1, .width = 128, .height = 160, .mipmaps = 1, .format = 0 };

    p.side = SIDE_TOP;
    p.handArea = (Rectangle){ 1740.0f, 0.0f, 180.0f, 1080.0f };
    p.handCards[0] = &cardA;
    p.handCards[3] = &cardB;

    reset_draw_capture();
    hand_ui_draw(&p, placeholder);

    assert(g_draw_texture_calls == 2);
    assert(approx_eq(g_drawn_rotation[0], 270.0f, 0.01f));
    assert(approx_eq(g_drawn_rotation[1], 270.0f, 0.01f));

    Vector2 expected0 = hand_ui_card_center_for_index(p.handArea, 2, 0);
    Vector2 expected1 = hand_ui_card_center_for_index(p.handArea, 2, 1);
    assert(approx_eq(g_drawn_dst[0].x, expected0.x, 0.01f));
    assert(approx_eq(g_drawn_dst[0].y, expected0.y, 0.01f));
    assert(approx_eq(g_drawn_dst[1].x, expected1.x, 0.01f));
    assert(approx_eq(g_drawn_dst[1].y, expected1.y, 0.01f));

    printf("  PASS: test_sparse_hand_compacts_draw_positions\n");
}

/* ---- main ---- */
int main(void) {
    printf("Running hand_ui tests...\n");
    test_center_x_matches_handarea_midline();
    test_cards_monotonic_y();
    test_even_stride();
    test_one_card_centered_in_handarea();
    test_three_card_run_centered_in_handarea();
    test_expected_absolute_positions_p1();
    test_empty_hand_draws_nothing();
    test_sparse_hand_compacts_draw_positions();
    printf("\nAll 8 tests passed!\n");
    return 0;
}
