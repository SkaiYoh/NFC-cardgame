/*
 * Unit tests for src/rendering/hand_ui.c
 *
 * Scope: pure layout math for hand_ui_card_center_for_index. We stub raylib
 * and the game type headers so the test compiles with only -lm, matching
 * the header-guard-override pattern used by test_battlefield.c and
 * test_spawn_fx.c.
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
static void DrawRectangleRec(Rectangle r, Color c) { (void)r; (void)c; }
static void DrawTexturePro(Texture2D t, Rectangle src, Rectangle dst,
                           Vector2 origin, float rotation, Color tint) {
    (void)t; (void)src; (void)dst; (void)origin; (void)rotation; (void)tint;
}

/* ---- Config stub: only the constants hand_ui.c depends on ---- */
#define NFC_CARDGAME_CONFIG_H
#define HAND_UI_DEPTH_PX               180
#define HAND_CARD_COUNT                8
#define HAND_CARD_WIDTH                128
#define HAND_CARD_HEIGHT               160
#define HAND_CARD_GAP                  4
#define HAND_CARD_PLACEHOLDER_PATH     "src/assets/cards/uvulite_card.png"

/* ---- Skip the heavy types.h chain ---- */
#define NFC_CARDGAME_TYPES_H

typedef enum { SIDE_BOTTOM = 0, SIDE_TOP = 1 } BattleSide;

typedef struct Player {
    int id;
    BattleSide side;
    Rectangle screenArea;
    Rectangle battlefieldArea;
    Rectangle handArea;
} Player;

/* ---- Skip hand_ui.h include chain by providing the declarations ourselves ---- */
#define NFC_CARDGAME_HAND_UI_H
Texture2D hand_ui_load_placeholder(void);
void hand_ui_unload_placeholder(Texture2D texture);
void hand_ui_draw(const Player *p, Texture2D placeholder);
Vector2 hand_ui_card_center_for_index(Rectangle handArea, int cardIndex);

/* ---- Production code under test ---- */
#include "../src/rendering/hand_ui.c"

/* ---- Test helpers ---- */

static bool approx_eq(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

/* Expected layout math (must match hand_ui.c):
 *   stride    = HAND_CARD_WIDTH + HAND_CARD_GAP                      = 132
 *   runLength = N*HAND_CARD_WIDTH + (N-1)*HAND_CARD_GAP               = 1052 (N=8)
 *   startY    = handArea.y + (handArea.height - runLength) * 0.5f
 *                           + HAND_CARD_WIDTH * 0.5f
 *   For {0,0,180,1080}: startY = 0 + (1080-1052)*0.5 + 64 = 14 + 64   = 78
 *   For P2 {1740,0,180,1080}: same Y, cx shifted to 1830.
 */

/* ---- Test: center X matches handArea midline for both seats ---- */
static void test_center_x_matches_handarea_midline(void) {
    Rectangle p1Hand = { 0.0f, 0.0f, 180.0f, 1080.0f };
    Rectangle p2Hand = { 1740.0f, 0.0f, 180.0f, 1080.0f };

    for (int i = 0; i < HAND_CARD_COUNT; i++) {
        Vector2 p1 = hand_ui_card_center_for_index(p1Hand, i);
        Vector2 p2 = hand_ui_card_center_for_index(p2Hand, i);
        assert(approx_eq(p1.x, p1Hand.x + p1Hand.width * 0.5f, 0.01f));
        assert(approx_eq(p2.x, p2Hand.x + p2Hand.width * 0.5f, 0.01f));
    }
    printf("  PASS: test_center_x_matches_handarea_midline\n");
}

/* ---- Test: card 0 sits above card 1 (monotonic Y) ---- */
static void test_cards_monotonic_y(void) {
    Rectangle hand = { 0.0f, 0.0f, 180.0f, 1080.0f };
    for (int i = 1; i < HAND_CARD_COUNT; i++) {
        Vector2 prev = hand_ui_card_center_for_index(hand, i - 1);
        Vector2 curr = hand_ui_card_center_for_index(hand, i);
        assert(curr.y > prev.y);
    }
    printf("  PASS: test_cards_monotonic_y\n");
}

/* ---- Test: centers are evenly spaced by stride ---- */
static void test_even_stride(void) {
    Rectangle hand = { 0.0f, 0.0f, 180.0f, 1080.0f };
    const float stride = (float)HAND_CARD_WIDTH + (float)HAND_CARD_GAP;
    for (int i = 1; i < HAND_CARD_COUNT; i++) {
        Vector2 prev = hand_ui_card_center_for_index(hand, i - 1);
        Vector2 curr = hand_ui_card_center_for_index(hand, i);
        assert(approx_eq(curr.y - prev.y, stride, 0.01f));
    }
    printf("  PASS: test_even_stride\n");
}

/* ---- Test: run is vertically centered inside handArea ---- */
static void test_run_centered_in_handarea(void) {
    Rectangle hand = { 0.0f, 0.0f, 180.0f, 1080.0f };

    Vector2 first = hand_ui_card_center_for_index(hand, 0);
    Vector2 last  = hand_ui_card_center_for_index(hand, HAND_CARD_COUNT - 1);

    const float halfCardWidth = (float)HAND_CARD_WIDTH * 0.5f;
    const float topEdge    = first.y - halfCardWidth;
    const float bottomEdge = last.y  + halfCardWidth;

    const float topSlack    = topEdge - hand.y;
    const float bottomSlack = (hand.y + hand.height) - bottomEdge;

    assert(approx_eq(topSlack, bottomSlack, 0.01f));
    // Sanity: top/bottom slack is positive (run fits inside the handArea).
    assert(topSlack > 0.0f);

    printf("  PASS: test_run_centered_in_handarea\n");
}

/* ---- Test: expected absolute values for P1 {0,0,180,1080} ---- */
static void test_expected_absolute_positions_p1(void) {
    Rectangle p1Hand = { 0.0f, 0.0f, 180.0f, 1080.0f };

    // startY = (1080 - 1052) / 2 + 64 = 14 + 64 = 78
    float startY = 78.0f;
    float stride = (float)HAND_CARD_WIDTH + (float)HAND_CARD_GAP; // 132

    for (int i = 0; i < HAND_CARD_COUNT; i++) {
        Vector2 c = hand_ui_card_center_for_index(p1Hand, i);
        assert(approx_eq(c.x, 90.0f, 0.01f));
        assert(approx_eq(c.y, startY + (float)i * stride, 0.01f));
    }
    printf("  PASS: test_expected_absolute_positions_p1\n");
}

/* ---- Test: expected absolute values for P2 {1740,0,180,1080} ---- */
static void test_expected_absolute_positions_p2(void) {
    Rectangle p2Hand = { 1740.0f, 0.0f, 180.0f, 1080.0f };

    float startY = 78.0f;
    float stride = (float)HAND_CARD_WIDTH + (float)HAND_CARD_GAP;

    for (int i = 0; i < HAND_CARD_COUNT; i++) {
        Vector2 c = hand_ui_card_center_for_index(p2Hand, i);
        assert(approx_eq(c.x, 1830.0f, 0.01f));
        assert(approx_eq(c.y, startY + (float)i * stride, 0.01f));
    }
    printf("  PASS: test_expected_absolute_positions_p2\n");
}

/* ---- main ---- */
int main(void) {
    printf("Running hand_ui tests...\n");
    test_center_x_matches_handarea_midline();
    test_cards_monotonic_y();
    test_even_stride();
    test_run_centered_in_handarea();
    test_expected_absolute_positions_p1();
    test_expected_absolute_positions_p2();
    printf("\nAll 6 tests passed!\n");
    return 0;
}
