#ifndef CARD_RENDER_H
#define CARD_RENDER_H

#include "raylib.h"
#include <stdbool.h>

/* Card canvas dimensions (body + margin for gems/decorations) */
#define CARD_WIDTH   64
#define CARD_HEIGHT  80
#define CARD_BODY_W  46
#define CARD_BODY_H  62
#define CARD_MARGIN  8

/* Sprite sheet path */
#define CARD_SHEET_PATH "assets/ModularCardsRPG/modularCardsRPGSheet.png"

/* Colors available for card assets (13 total) */
typedef enum {
    CLR_AQUA, CLR_BLACK, CLR_BLUE, CLR_BLUE_LIGHT, CLR_BROWN,
    CLR_GRAY, CLR_GREEN, CLR_MAGENTA, CLR_PINK, CLR_PURPLE,
    CLR_RED, CLR_WHITE, CLR_YELLOW,
    CLR_COUNT
} CardColor;

/* Background / description styles (4 options) */
typedef enum {
    BG_BLACK, BG_BROWN, BG_PAPER, BG_WHITE,
    BG_COUNT
} BGStyle;

/* Container variant */
typedef enum {
    CONTAINER_1, CONTAINER_2, CONTAINER_3,
    CONTAINER_COUNT
} ContainerVariant;

/* Inner corner styles */
typedef enum {
    IC_BLACK, IC_BROWN, IC_WHITE, IC_YELLOW,
    IC_COUNT
} InnerCornerStyle;

/* Number of drawable layers */
#define CARD_LAYER_COUNT 11

/* Per-layer position offsets for fine-tuning composition */
typedef struct {
    float x[CARD_LAYER_COUNT];
    float y[CARD_LAYER_COUNT];
} CardLayerOffsets;

/* Visual recipe describing how to compose a card face */
typedef struct {
    CardColor        border_color;
    bool             show_border;
    BGStyle          bg_style;
    bool             show_bg;
    CardColor        banner_color;
    bool             show_banner;
    CardColor        corner_color;
    bool             show_corner;
    CardColor        container_color;
    ContainerVariant container_variant;
    bool             show_container;
    BGStyle          description_style;
    bool             show_description;
    InnerCornerStyle innercorner_style;
    bool             show_innercorner;
    CardColor        gem_color;
    bool             show_gem;
    CardColor        socket_color;
    bool             show_socket;
    CardColor        energy_top_color;
    bool             show_energy_top;
    CardColor        energy_bot_color;
    bool             show_energy_bot;

    /* Per-layer position offsets (persisted) */
    CardLayerOffsets  offsets;
} CardVisual;

/* Atlas: single sprite sheet texture + source rectangles for every asset */
typedef struct {
    Texture2D sheet;  /* the single loaded sprite sheet */

    /* Source rectangles into the sheet, indexed by color/style */
    Rectangle borders[CLR_COUNT];
    Rectangle corners[CLR_COUNT];
    Rectangle backs[CLR_COUNT];
    Rectangle banners[CLR_COUNT];
    Rectangle bgs[BG_COUNT];
    Rectangle descriptions[BG_COUNT];
    Rectangle containers[CLR_COUNT][CONTAINER_COUNT];
    Rectangle inner_corners[IC_COUNT];
    Rectangle gems[CLR_COUNT];
    Rectangle sockets[CLR_COUNT];
    Rectangle energy_top[CLR_COUNT];
    Rectangle energy_bot[CLR_COUNT];
    Rectangle energy_full[CLR_COUNT];
} CardAtlas;

/* Load sprite sheet and initialize all source rectangles */
void card_atlas_init(CardAtlas *atlas);

/* Unload the sprite sheet texture */
void card_atlas_free(CardAtlas *atlas);

/* Draw a composed card face at pos (top-left) with scale */
void card_draw(const CardAtlas *atlas, const CardVisual *visual,
               Vector2 pos, float scale);

/* Draw with per-layer position offsets (for preview tool tweaking) */
void card_draw_ex(const CardAtlas *atlas, const CardVisual *visual,
                  const CardLayerOffsets *offsets, Vector2 pos, float scale);

/* Get default (zero) layer offsets */
CardLayerOffsets card_layer_offsets_default(void);

/* Draw a card back at pos (top-left) with scale */
void card_draw_back(const CardAtlas *atlas, CardColor color,
                    Vector2 pos, float scale);

/* Get a sensible default visual */
CardVisual card_visual_default(void);

/* Parse a CardVisual from a card's JSON data string.
 * Looks for a "visual" key. Returns defaults if missing/invalid. */
CardVisual card_visual_from_json(const char *json_data);

/* Print the CardVisual as a JSON snippet to stdout */
void card_visual_print_json(const CardVisual *visual);

/* Human-readable name strings */
const char *card_color_name(CardColor c);
const char *bg_style_name(BGStyle s);
const char *container_variant_name(ContainerVariant v);
const char *innercorner_style_name(InnerCornerStyle s);

#endif /* CARD_RENDER_H */
