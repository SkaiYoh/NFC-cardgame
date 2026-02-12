#include "card_render.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── name tables ────────────────────────────────────────────────── */

static const char *clr_names[CLR_COUNT] = {
    "aqua", "black", "blue", "blue_light", "brown",
    "gray", "green", "magenta", "pink", "purple",
    "red", "white", "yellow"
};

static const char *bg_names[BG_COUNT] = {
    "black", "brown", "paper", "white"
};

static const char *ic_names[IC_COUNT] = {
    "black", "brown", "white", "yellow"
};

/*
 * 11-color set (border/corner/back/banner/container):
 *   aqua, black, blue, brown, gray, green, magenta, purple, red, white, yellow
 *   (NO blue_light or pink)
 *
 * 13-color set (gem/socket/energy): all of the above + blue_light, pink
 */

const char *card_color_name(CardColor c) {
    return (c >= 0 && c < CLR_COUNT) ? clr_names[c] : "unknown";
}

const char *bg_style_name(BGStyle s) {
    return (s >= 0 && s < BG_COUNT) ? bg_names[s] : "unknown";
}

const char *container_variant_name(ContainerVariant v) {
    static const char *names[] = { "1", "2", "3" };
    return (v >= 0 && v < CONTAINER_COUNT) ? names[v] : "?";
}

const char *innercorner_style_name(InnerCornerStyle s) {
    return (s >= 0 && s < IC_COUNT) ? ic_names[s] : "unknown";
}

/* ── sprite sheet coordinate mapping ────────────────────────────── */
/*
 * Sprite sheet: modularCardsRPGSheet.png (464 × 384 px)
 *
 * YOU MUST FILL IN the exact (x, y) pixel coordinates for each asset
 * on the sheet. Each Rectangle is { x, y, width, height }.
 *
 * Asset sizes for reference:
 *   corner                 : 48 × 64
 *   border                 : 46 × 62
 *   back                   : 42 × 58
 *   BG                     : 38 × 25
 *   description             : 38 × 19
 *   banner                 : 48 × 11
 *   innercorner            : 38 × 6
 *   container 1,2          : 14 × 14
 *   container 3            : 16 × 16
 *   energy                 : 16 × 16
 *   gem / socket           :  8 ×  8
 *
 * Use a pixel editor to find the top-left (x,y) of each asset on the
 * sheet, then set the Rectangle accordingly.  A {0,0,0,0} rect means
 * "not mapped yet" and will be skipped when drawing.
 */

/* Helper: shorthand for building source rects */
#define R(x, y, w, h)  (Rectangle){ (x), (y), (w), (h) }
#define EMPTY           (Rectangle){ 0, 0, 0, 0 }

static void init_rects(CardAtlas *atlas) {
    /* Zero everything first so unmapped entries are {0,0,0,0} */
    memset(atlas->borders,       0, sizeof(atlas->borders));
    memset(atlas->corners,       0, sizeof(atlas->corners));
    memset(atlas->backs,         0, sizeof(atlas->backs));
    memset(atlas->banners,       0, sizeof(atlas->banners));
    memset(atlas->bgs,           0, sizeof(atlas->bgs));
    memset(atlas->descriptions,  0, sizeof(atlas->descriptions));
    memset(atlas->containers,    0, sizeof(atlas->containers));
    memset(atlas->inner_corners, 0, sizeof(atlas->inner_corners));
    memset(atlas->gems,          0, sizeof(atlas->gems));
    memset(atlas->sockets,       0, sizeof(atlas->sockets));
    memset(atlas->energy_top,    0, sizeof(atlas->energy_top));
    memset(atlas->energy_bot,    0, sizeof(atlas->energy_bot));
    memset(atlas->energy_full,   0, sizeof(atlas->energy_full));

    /*
     * ── FILL IN COORDINATES BELOW ──
     *
     * Format:  atlas->category[ENUM_VALUE] = R(x, y, w, h);
     *
     * Example (hypothetical):
     *   atlas->borders[CLR_RED]   = R(0, 0, 46, 62);
     *   atlas->borders[CLR_BLUE]  = R(0, 64, 46, 62);
     *   atlas->gems[CLR_RED]      = R(350, 0, 8, 8);
     *   ...
     */

    /* ── borders (46×62) ── */
    atlas->borders[CLR_RED]      = R(1, 1, 46, 62);
    atlas->borders[CLR_YELLOW]   = R(1, 65, 46, 62);
    atlas->borders[CLR_GREEN]    = R(1, 129, 46, 62);
    atlas->borders[CLR_BLUE]     = R(1, 193, 46, 62);
    atlas->borders[CLR_PURPLE]   = R(1, 257, 46, 62);
    atlas->borders[CLR_MAGENTA]  = R(1, 321, 46, 62);
    atlas->borders[CLR_AQUA]     = R(49, 1, 46, 62);
    atlas->borders[CLR_BROWN]    = R(49, 65, 46, 62);
    atlas->borders[CLR_BLACK]    = R(49, 129, 46, 62);
    atlas->borders[CLR_GRAY]     = R(49, 193, 46, 62);
    atlas->borders[CLR_WHITE]    = R(49, 257, 46, 62);

    /* ── corners (48×64) ── */
    atlas->corners[CLR_RED]      = R(96, 0, 48, 64);
    atlas->corners[CLR_YELLOW]   = R(96, 64, 48, 64);
    atlas->corners[CLR_GREEN]    = R(96, 128, 48, 64);
    atlas->corners[CLR_BLUE]     = R(96, 192, 48, 64);
    atlas->corners[CLR_PURPLE]   = R(96, 256, 48, 64);
    atlas->corners[CLR_MAGENTA]  = R(48, 320, 48, 64);
    atlas->corners[CLR_AQUA]     = R(144, 0, 48, 64);
    atlas->corners[CLR_BROWN]    = R(144, 64, 48, 64);
    atlas->corners[CLR_BLACK]    = R(144, 128, 48, 64);
    atlas->corners[CLR_GRAY]     = R(144, 192, 48, 64);
    atlas->corners[CLR_WHITE]    = R(144, 256, 48, 64);

    /* ── backs (42×58) ── */
    atlas->backs[CLR_RED]      = R(195, 3, 42, 58);
    atlas->backs[CLR_YELLOW]   = R(195, 67, 42, 58);
    atlas->backs[CLR_GREEN]    = R(195, 131, 42, 58);
    atlas->backs[CLR_BLUE]     = R(195, 195, 42, 58);
    atlas->backs[CLR_PURPLE]   = R(195, 259, 42, 58);
    atlas->backs[CLR_MAGENTA]  = R(195, 323, 42, 58);
    atlas->backs[CLR_AQUA]     = R(243, 3, 42, 58);
    atlas->backs[CLR_BROWN]    = R(243, 67, 42, 58);
    atlas->backs[CLR_BLACK]    = R(243, 131, 42, 58);
    atlas->backs[CLR_GRAY]     = R(243, 195, 42, 58);
    atlas->backs[CLR_WHITE]    = R(243, 259, 42, 58);

    /* ── banners (48×11) ── */
    atlas->banners[CLR_MAGENTA]  = R(288, 2, 48, 11);
    atlas->banners[CLR_RED]      = R(288, 18, 48, 11);
    atlas->banners[CLR_YELLOW]   = R(288, 34, 48, 11);
    atlas->banners[CLR_GREEN]    = R(288, 50, 48, 11);
    atlas->banners[CLR_AQUA]     = R(288, 66, 48, 11);
    atlas->banners[CLR_BLUE]     = R(288, 82, 48, 11);
    atlas->banners[CLR_PURPLE]   = R(288, 98, 48, 11);
    atlas->banners[CLR_BROWN]    = R(288, 114, 48, 11);
    atlas->banners[CLR_BLACK]    = R(288, 130, 48, 11);
    atlas->banners[CLR_GRAY]     = R(288, 146, 48, 11);
    atlas->banners[CLR_WHITE]    = R(288, 162, 48, 11);

    /* ── bgs (38x25) ── */
    atlas->bgs[BG_BLACK]         = R(341, 277, 38, 25);
    atlas->bgs[BG_BROWN]         = R(341, 181, 38, 25);
    atlas->bgs[BG_PAPER]         = R(341, 213, 38, 25);
    atlas->bgs[BG_WHITE]         = R(341, 245, 38, 25);

    /* ── descriptions (38x19) ── */
    atlas->descriptions[BG_BROWN]  = R(293, 184, 38, 19);
    atlas->descriptions[BG_PAPER]  = R(293, 216, 38, 19);
    atlas->descriptions[BG_WHITE]  = R(293, 248, 38, 19);
    atlas->descriptions[BG_BLACK]  = R(293, 280, 38, 19);

    /* ── containers (16×16) — 11 colors × 3 variants ── */
    atlas->containers[CLR_MAGENTA][CONTAINER_1]  = R(401, 1, 14, 14);
    atlas->containers[CLR_RED][CONTAINER_1]      = R(401, 17, 14, 14);
    atlas->containers[CLR_YELLOW][CONTAINER_1]    = R(401, 33, 14, 14);
    atlas->containers[CLR_GREEN][CONTAINER_1]    = R(401, 49, 14, 14);
    atlas->containers[CLR_AQUA][CONTAINER_1]     = R(401, 65, 14, 14);
    atlas->containers[CLR_BLUE][CONTAINER_1]     = R(401, 81, 14, 14);
    atlas->containers[CLR_PURPLE][CONTAINER_1]   = R(401, 97, 14, 14);
    atlas->containers[CLR_BROWN][CONTAINER_1]    = R(401, 113, 14, 14);
    atlas->containers[CLR_BLACK][CONTAINER_1]    = R(401, 129, 14, 14);
    atlas->containers[CLR_GRAY][CONTAINER_1]     = R(401, 145, 14, 14);
    atlas->containers[CLR_WHITE][CONTAINER_1]    = R(401, 161, 14, 14);

    atlas->containers[CLR_MAGENTA][CONTAINER_2]  = R(417, 1, 14, 14);
    atlas->containers[CLR_RED][CONTAINER_2]      = R(417, 17, 14, 14);
    atlas->containers[CLR_YELLOW][CONTAINER_2]   = R(417, 33, 14, 14);
    atlas->containers[CLR_GREEN][CONTAINER_2]    = R(417, 49, 14, 14);
    atlas->containers[CLR_AQUA][CONTAINER_2]     = R(417, 65, 14, 14);
    atlas->containers[CLR_BLUE][CONTAINER_2]     = R(417, 81, 14, 14);
    atlas->containers[CLR_PURPLE][CONTAINER_2]   = R(417, 97, 14, 14);
    atlas->containers[CLR_BROWN][CONTAINER_2]    = R(417, 113, 14, 14);
    atlas->containers[CLR_BLACK][CONTAINER_2]    = R(417, 129, 14, 14);
    atlas->containers[CLR_GRAY][CONTAINER_2]     = R(417, 145, 14, 14);
    atlas->containers[CLR_WHITE][CONTAINER_2]    = R(417, 161, 14, 14);

    atlas->containers[CLR_MAGENTA][CONTAINER_3]  = R(432, 0, 16, 16);
    atlas->containers[CLR_RED][CONTAINER_3]      = R(432, 16, 16, 16);
    atlas->containers[CLR_YELLOW][CONTAINER_3]   = R(432, 32, 16, 16);
    atlas->containers[CLR_GREEN][CONTAINER_3]    = R(432, 48, 16, 16);
    atlas->containers[CLR_AQUA][CONTAINER_3]     = R(432, 64, 16, 16);
    atlas->containers[CLR_BLUE][CONTAINER_3]     = R(432, 80, 16, 16);
    atlas->containers[CLR_PURPLE][CONTAINER_3]   = R(432, 96, 16, 16);
    atlas->containers[CLR_BROWN][CONTAINER_3]    = R(432, 112, 16, 16);
    atlas->containers[CLR_BLACK][CONTAINER_3]    = R(432, 128, 16, 16);
    atlas->containers[CLR_GRAY][CONTAINER_3]     = R(432, 144, 16, 16);
    atlas->containers[CLR_WHITE][CONTAINER_3]    = R(432, 160, 16, 16);

    /* ── inner corners (48×16) ── */
    atlas->inner_corners[IC_BROWN]  = R(389, 181, 38, 6);
    atlas->inner_corners[IC_BLACK]  = R(389, 197, 38, 6);
    atlas->inner_corners[IC_YELLOW] = R(389, 213, 38, 6);
    atlas->inner_corners[IC_WHITE]  = R(389, 229, 38, 6);

    /* ── gems (4×4) — all 13 colors ── */
    atlas->gems[CLR_RED]         = R(450, 58, 4, 4);
    atlas->gems[CLR_GREEN]       = R(450, 66, 4, 4);
    atlas->gems[CLR_BLUE]        = R(450, 74, 4, 4);
    atlas->gems[CLR_BROWN]       = R(450, 82, 4, 4);
    atlas->gems[CLR_BLACK]       = R(450, 90, 4, 4);
    atlas->gems[CLR_MAGENTA]     = R(450, 98, 4, 4);
    atlas->gems[CLR_PINK]        = R(458, 50, 4, 4);
    atlas->gems[CLR_YELLOW]      = R(458, 58, 4, 4);
    atlas->gems[CLR_AQUA]        = R(458, 66, 4, 4);
    atlas->gems[CLR_PURPLE]      = R(458, 74, 4, 4);
    atlas->gems[CLR_WHITE]       = R(458, 82, 4, 4);
    atlas->gems[CLR_GRAY]        = R(458, 90, 4, 4);
    atlas->gems[CLR_BLUE_LIGHT]  = R(458, 98, 4, 4);

    /* ── sockets (8×8) — all 13 colors ── */
    // atlas->sockets[CLR_AQUA]    = R(?, ?, 8, 8);
    atlas->sockets[CLR_RED]         = R(448, 0, 8, 8);
    atlas->sockets[CLR_GREEN]       = R(448, 8, 8, 8);
    atlas->sockets[CLR_BLUE]        = R(448, 16, 8, 8);
    atlas->sockets[CLR_BROWN]       = R(448, 24, 8, 8);
    atlas->sockets[CLR_BLACK]       = R(448, 32, 8, 8);
    atlas->sockets[CLR_MAGENTA]     = R(448, 40, 8, 8);
    atlas->sockets[CLR_PINK]        = R(448, 48, 8, 8);
    atlas->sockets[CLR_YELLOW]      = R(456, 0, 8, 8);
    atlas->sockets[CLR_AQUA]        = R(456, 8, 8, 8);
    atlas->sockets[CLR_PURPLE]      = R(456, 16, 8, 8);
    atlas->sockets[CLR_WHITE]       = R(456, 24, 8, 8);
    atlas->sockets[CLR_GRAY]        = R(456, 32, 8, 8);
    atlas->sockets[CLR_BLUE_LIGHT]  = R(456, 40, 8, 8);
    // ... (all 13)

    /* ── energy_top (12×12) — all 13 colors ── */
    // atlas->energy_top[CLR_AQUA] = R(?, ?, 12, 12);
    atlas->energy_top[CLR_MAGENTA]     = R(338, 2, 12, 12);
    atlas->energy_top[CLR_RED]         = R(338, 18, 12, 12);
    atlas->energy_top[CLR_YELLOW]      = R(338, 34, 12, 12);
    atlas->energy_top[CLR_GREEN]       = R(338, 50, 12, 12);
    atlas->energy_top[CLR_AQUA]        = R(338, 66, 12, 12);
    atlas->energy_top[CLR_BLUE]        = R(338, 82, 12, 12);
    atlas->energy_top[CLR_PURPLE]      = R(338, 98, 12, 12);
    atlas->energy_top[CLR_BROWN]       = R(338, 114, 12, 12);
    atlas->energy_top[CLR_BLACK]       = R(338, 130, 12, 12);
    atlas->energy_top[CLR_GRAY]        = R(338, 146, 12, 12);
    atlas->energy_top[CLR_WHITE]       = R(338, 162, 12, 12);
    atlas->energy_top[CLR_BLUE_LIGHT]  = R(370, 34, 12, 12);
    atlas->energy_top[CLR_PINK]        = R(370, 50, 12, 12);
    // ...

    /* ── energy_bot (12×12) — all 13 colors ── */
    atlas->energy_bot[CLR_MAGENTA]     = R(354, 2, 12, 12);
    atlas->energy_bot[CLR_RED]         = R(354, 18, 12, 12);
    atlas->energy_bot[CLR_YELLOW]      = R(354, 34, 12, 12);
    atlas->energy_bot[CLR_GREEN]       = R(354, 50, 12, 12);
    atlas->energy_bot[CLR_AQUA]        = R(354, 66, 12, 12);
    atlas->energy_bot[CLR_BLUE]        = R(354, 82, 12, 12);
    atlas->energy_bot[CLR_PURPLE]      = R(354, 98, 12, 12);
    atlas->energy_bot[CLR_BROWN]       = R(354, 114, 12, 12);
    atlas->energy_bot[CLR_BLACK]       = R(354, 130, 12, 12);
    atlas->energy_bot[CLR_GRAY]        = R(354, 146, 12, 12);
    atlas->energy_bot[CLR_WHITE]       = R(354, 162, 12, 12);
    atlas->energy_bot[CLR_BLUE_LIGHT]  = R(386, 34, 12, 12);
    atlas->energy_bot[CLR_PINK]        = R(386, 50, 12, 12);
    // ...

    /* ── energy_full (16×16) — all 13 colors ── */
    // atlas->energy_full[CLR_AQUA] = R(?, ?, 16, 16);
    // ...
}

/* ── atlas init / free ──────────────────────────────────────────── */

void card_atlas_init(CardAtlas *atlas) {
    memset(atlas, 0, sizeof(CardAtlas));
    atlas->sheet = LoadTexture(CARD_SHEET_PATH);
    SetTextureFilter(atlas->sheet, TEXTURE_FILTER_POINT);
    init_rects(atlas);
    printf("Card atlas loaded (sheet %dx%d)\n", atlas->sheet.width, atlas->sheet.height);
}

void card_atlas_free(CardAtlas *atlas) {
    if (atlas->sheet.id) UnloadTexture(atlas->sheet);
    memset(atlas, 0, sizeof(CardAtlas));
}

/* ── drawing ────────────────────────────────────────────────────── */

/* Draw a single layer from the sprite sheet at an offset within the card */
static void draw_layer(Texture2D sheet, Rectangle src,
                       Vector2 card_pos, float ox, float oy, float scale) {
    /* Skip unmapped rects (width or height == 0) */
    if (src.width == 0 || src.height == 0) return;

    DrawTexturePro(sheet, src,
        (Rectangle){
            card_pos.x + ox * scale,
            card_pos.y + oy * scale,
            src.width  * scale,
            src.height * scale
        },
        (Vector2){ 0, 0 }, 0.0f, WHITE);
}

/*
 * Layer composition (back-to-front):
 *
 * Card canvas = 64×80.  Body elements are 48×64, offset by (8,8).
 *
 *   ┌─────────── 64 ───────────┐
 *   │ 8px margin               │
 *   │  ┌────── 48 ──────┐     │  ← y=8
 *   │  │ BG (48×32)      │     │
 *   │  │                 │     │
 *   │  ├─────────────────┤     │  ← y=40
 *   │  │ Desc (48×32)    │     │
 *   │  │                 │     │
 *   │  └─────────────────┘     │  ← y=72
 *   │                          │
 *   └──────────────────────────┘  ← y=80
 *
 *   Border & corner overlays cover the full body (48×64 at 8,8).
 *   Banner (48×16) sits at the seam between BG and description.
 *   Gem/socket at top-left corner of border (~4,4).
 *   Energy indicator near the gem area.
 */

/*
 * Layer indices (match CARD_LAYER_COUNT = 11):
 *   0=BG, 1=Description, 2=Border, 3=Banner, 4=InnerCorner,
 *   5=Corner, 6=Container, 7=Socket, 8=Gem, 9=EnergyTop, 10=EnergyBot
 */

/* Base positions for each layer (before offsets) */
static void get_base_positions(float *ox, float *oy) {
    float bx = CARD_MARGIN;
    float by = CARD_MARGIN;

    /* BG centered: 38px wide on 46px body → +4x, 2px below body top */
    ox[0] = bx + (CARD_BODY_W - 38) / 2.0f;  oy[0] = by + 4;
    /* Description centered: 38px wide on 46px body → +4x */
    ox[1] = bx + (CARD_BODY_W - 38) / 2.0f;    oy[1] = by + 32 + 7;
    /* Border */
    ox[2] = bx;                                 oy[2] = by;
    /* Banner centered: 48px on 46px body → -1x, vertically on seam */
    ox[3] = bx - 1;                             oy[3] = by + 27 + 2;
    /* Inner corner centered: 38px wide on 46px body → +4x, moved up 20px */
    ox[4] = bx + (CARD_BODY_W - 38) / 2.0f;    oy[4] = by + 24 - 20;
    /* Corner (-1,-1 to center on border) */
    ox[5] = bx - 1;                             oy[5] = by - 1;
    /* Container (14×14) offset +3,+3 from origin */
    ox[6] = 3;                                  oy[6] = 3;
    /* Socket (8×8) */
    ox[7] = 4;                                  oy[7] = 4;
    /* Gem (4×4) centered on socket */
    ox[8] = 4 + (8 - 4) / 2.0f;                oy[8] = 4 + (8 - 4) / 2.0f;
    /* Energy top (12×12) centered on container (14×14 at ox[6],oy[6]) */
    ox[9] = ox[6] + (14 - 12) / 2.0f;          oy[9] = oy[6] + (14 - 12) / 2.0f;
    /* Energy bottom (12×12) centered on container */
    ox[10] = ox[6] + (14 - 12) / 2.0f;         oy[10] = oy[6] + (14 - 12) / 2.0f;
}

CardLayerOffsets card_layer_offsets_default(void) {
    CardLayerOffsets o = {0};
    return o;
}

void card_draw_ex(const CardAtlas *atlas, const CardVisual *visual,
                  const CardLayerOffsets *offsets, Vector2 pos, float scale) {
    Texture2D s = atlas->sheet;
    float ox[CARD_LAYER_COUNT], oy[CARD_LAYER_COUNT];
    get_base_positions(ox, oy);

    /* Apply per-layer offsets */
    if (offsets) {
        for (int i = 0; i < CARD_LAYER_COUNT; i++) {
            ox[i] += offsets->x[i];
            oy[i] += offsets->y[i];
        }
    }

    /* 0. Background */
    if (visual->show_bg)
        draw_layer(s, atlas->bgs[visual->bg_style], pos, ox[0], oy[0], scale);

    /* 1. Description */
    if (visual->show_description)
        draw_layer(s, atlas->descriptions[visual->description_style], pos, ox[1], oy[1], scale);

    /* 2. Border frame */
    if (visual->show_border)
        draw_layer(s, atlas->borders[visual->border_color], pos, ox[2], oy[2], scale);

    /* 3. Banner */
    if (visual->show_banner)
        draw_layer(s, atlas->banners[visual->banner_color], pos, ox[3], oy[3], scale);

    /* 4. Inner corner accents */
    if (visual->show_innercorner)
        draw_layer(s, atlas->inner_corners[visual->innercorner_style], pos, ox[4], oy[4], scale);

    /* 5. Decorative corners */
    if (visual->show_corner)
        draw_layer(s, atlas->corners[visual->corner_color], pos, ox[5], oy[5], scale);

    /* 6. Container */
    if (visual->show_container)
        draw_layer(s, atlas->containers[visual->container_color][visual->container_variant],
                   pos, ox[6], oy[6], scale);

    /* 7. Socket (behind gem) */
    if (visual->show_socket)
        draw_layer(s, atlas->sockets[visual->socket_color], pos, ox[7], oy[7], scale);

    /* 8. Gem */
    if (visual->show_gem)
        draw_layer(s, atlas->gems[visual->gem_color], pos, ox[8], oy[8], scale);

    /* 9. Energy top */
    if (visual->show_energy_top)
        draw_layer(s, atlas->energy_top[visual->energy_top_color], pos, ox[9], oy[9], scale);

    /* 10. Energy bottom */
    if (visual->show_energy_bot)
        draw_layer(s, atlas->energy_bot[visual->energy_bot_color], pos, ox[10], oy[10], scale);
}

void card_draw(const CardAtlas *atlas, const CardVisual *visual,
               Vector2 pos, float scale) {
    card_draw_ex(atlas, visual, &visual->offsets, pos, scale);
}

void card_draw_back(const CardAtlas *atlas, CardColor color,
                    Vector2 pos, float scale) {
    /* Back is 42×58, centered within the 46×62 body area */
    float ox = CARD_MARGIN + (CARD_BODY_W - 42) / 2.0f;
    float oy = CARD_MARGIN + (CARD_BODY_H - 58) / 2.0f;
    draw_layer(atlas->sheet, atlas->backs[color], pos, ox, oy, scale);
}

CardVisual card_visual_default(void) {
    return (CardVisual){
        .border_color      = CLR_BROWN,
        .show_border       = true,
        .bg_style          = BG_PAPER,
        .show_bg           = true,
        .banner_color      = CLR_BROWN,
        .show_banner       = true,
        .corner_color      = CLR_BROWN,
        .show_corner       = true,
        .container_color   = CLR_BROWN,
        .container_variant = CONTAINER_1,
        .show_container    = false,
        .description_style = BG_PAPER,
        .show_description  = true,
        .innercorner_style = IC_BROWN,
        .show_innercorner  = true,
        .gem_color         = CLR_GREEN,
        .show_gem          = true,
        .socket_color      = CLR_BROWN,
        .show_socket       = true,
        .energy_top_color  = CLR_RED,
        .show_energy_top   = false,
        .energy_bot_color  = CLR_RED,
        .show_energy_bot   = false,
    };
}

/* ── JSON parsing ───────────────────────────────────────────────── */

static CardColor parse_color(const char *name) {
    if (!name) return CLR_BROWN;
    for (int i = 0; i < CLR_COUNT; i++) {
        if (strcmp(name, clr_names[i]) == 0) return (CardColor)i;
    }
    return CLR_BROWN;
}

static BGStyle parse_bg(const char *name) {
    if (!name) return BG_PAPER;
    for (int i = 0; i < BG_COUNT; i++) {
        if (strcmp(name, bg_names[i]) == 0) return (BGStyle)i;
    }
    return BG_PAPER;
}

static InnerCornerStyle parse_ic(const char *name) {
    if (!name) return IC_BROWN;
    for (int i = 0; i < IC_COUNT; i++) {
        if (strcmp(name, ic_names[i]) == 0) return (InnerCornerStyle)i;
    }
    return IC_BROWN;
}

CardVisual card_visual_from_json(const char *json_data) {
    CardVisual v = card_visual_default();
    if (!json_data) return v;

    cJSON *root = cJSON_Parse(json_data);

    /* If parse failed or result isn't an object, try wrapping in { }
     * (handles bare "visual": {...} format from export) */
    if (!root || !cJSON_IsObject(root)) {
        if (root) cJSON_Delete(root);
        size_t len = strlen(json_data);
        char *wrapped = malloc(len + 3);
        if (!wrapped) return v;
        wrapped[0] = '{';
        memcpy(wrapped + 1, json_data, len);
        wrapped[len + 1] = '}';
        wrapped[len + 2] = '\0';
        root = cJSON_Parse(wrapped);
        free(wrapped);
        if (!root) return v;
    }

    cJSON *vis = cJSON_GetObjectItemCaseSensitive(root, "visual");
    if (!vis || !cJSON_IsObject(vis)) {
        cJSON_Delete(root);
        return v;
    }

    cJSON *item;
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "border_color")))
        v.border_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_border")) && cJSON_IsBool(item))
        v.show_border = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "bg_style")))
        v.bg_style = parse_bg(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_bg")) && cJSON_IsBool(item))
        v.show_bg = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "banner_color")))
        v.banner_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_banner")) && cJSON_IsBool(item))
        v.show_banner = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "corner_color")))
        v.corner_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_corner")) && cJSON_IsBool(item))
        v.show_corner = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "container_color")))
        v.container_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "container_variant")) && cJSON_IsNumber(item)) {
        int cv = item->valueint;
        if (cv >= 1 && cv <= 3) v.container_variant = (ContainerVariant)(cv - 1);
    }
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_container")) && cJSON_IsBool(item))
        v.show_container = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "description_style")))
        v.description_style = parse_bg(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_description")) && cJSON_IsBool(item))
        v.show_description = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "innercorner_style")))
        v.innercorner_style = parse_ic(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_innercorner")) && cJSON_IsBool(item))
        v.show_innercorner = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "gem_color")))
        v.gem_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_gem")) && cJSON_IsBool(item))
        v.show_gem = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "socket_color")))
        v.socket_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_socket")) && cJSON_IsBool(item))
        v.show_socket = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "energy_top_color")))
        v.energy_top_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_energy_top")) && cJSON_IsBool(item))
        v.show_energy_top = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "energy_bot_color")))
        v.energy_bot_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_energy_bot")) && cJSON_IsBool(item))
        v.show_energy_bot = cJSON_IsTrue(item);

    /* Per-layer offsets */
    static const char *offset_keys[CARD_LAYER_COUNT] = {
        "off_bg", "off_description", "off_border", "off_banner",
        "off_innercorner", "off_corner", "off_container",
        "off_socket", "off_gem", "off_energy_top", "off_energy_bot"
    };
    for (int i = 0; i < CARD_LAYER_COUNT; i++) {
        cJSON *off = cJSON_GetObjectItemCaseSensitive(vis, offset_keys[i]);
        if (off && cJSON_IsArray(off) && cJSON_GetArraySize(off) == 2) {
            cJSON *ox = cJSON_GetArrayItem(off, 0);
            cJSON *oy = cJSON_GetArrayItem(off, 1);
            if (cJSON_IsNumber(ox)) v.offsets.x[i] = (float)ox->valuedouble;
            if (cJSON_IsNumber(oy)) v.offsets.y[i] = (float)oy->valuedouble;
        }
    }

    cJSON_Delete(root);
    return v;
}

void card_visual_print_json(const CardVisual *visual) {
    printf("{\n\"visual\": {\n");
    printf("  \"border_color\": \"%s\",\n",      card_color_name(visual->border_color));
    printf("  \"show_border\": %s,\n",            visual->show_border ? "true" : "false");
    printf("  \"bg_style\": \"%s\",\n",           bg_style_name(visual->bg_style));
    printf("  \"show_bg\": %s,\n",                visual->show_bg ? "true" : "false");
    printf("  \"banner_color\": \"%s\",\n",       card_color_name(visual->banner_color));
    printf("  \"show_banner\": %s,\n",            visual->show_banner ? "true" : "false");
    printf("  \"corner_color\": \"%s\",\n",       card_color_name(visual->corner_color));
    printf("  \"show_corner\": %s,\n",            visual->show_corner ? "true" : "false");
    printf("  \"container_color\": \"%s\",\n",    card_color_name(visual->container_color));
    printf("  \"container_variant\": %d,\n",      visual->container_variant + 1);
    printf("  \"show_container\": %s,\n",         visual->show_container ? "true" : "false");
    printf("  \"description_style\": \"%s\",\n",  bg_style_name(visual->description_style));
    printf("  \"show_description\": %s,\n",       visual->show_description ? "true" : "false");
    printf("  \"innercorner_style\": \"%s\",\n",  innercorner_style_name(visual->innercorner_style));
    printf("  \"show_innercorner\": %s,\n",       visual->show_innercorner ? "true" : "false");
    printf("  \"gem_color\": \"%s\",\n",          card_color_name(visual->gem_color));
    printf("  \"show_gem\": %s,\n",               visual->show_gem ? "true" : "false");
    printf("  \"socket_color\": \"%s\",\n",       card_color_name(visual->socket_color));
    printf("  \"show_socket\": %s,\n",            visual->show_socket ? "true" : "false");
    printf("  \"energy_top_color\": \"%s\",\n",   card_color_name(visual->energy_top_color));
    printf("  \"show_energy_top\": %s,\n",        visual->show_energy_top ? "true" : "false");
    printf("  \"energy_bot_color\": \"%s\",\n",   card_color_name(visual->energy_bot_color));
    static const char *offset_keys[CARD_LAYER_COUNT] = {
        "off_bg", "off_description", "off_border", "off_banner",
        "off_innercorner", "off_corner", "off_container",
        "off_socket", "off_gem", "off_energy_top", "off_energy_bot"
    };
    /* Count non-zero offsets to handle trailing comma */
    int off_count = 0;
    for (int i = 0; i < CARD_LAYER_COUNT; i++) {
        if (visual->offsets.x[i] != 0.0f || visual->offsets.y[i] != 0.0f)
            off_count++;
    }
    if (off_count == 0) {
        printf("  \"show_energy_bot\": %s\n",          visual->show_energy_bot ? "true" : "false");
    } else {
        printf("  \"show_energy_bot\": %s,\n",         visual->show_energy_bot ? "true" : "false");
        int printed = 0;
        for (int i = 0; i < CARD_LAYER_COUNT; i++) {
            if (visual->offsets.x[i] != 0.0f || visual->offsets.y[i] != 0.0f) {
                printed++;
                printf("  \"%s\": [%.0f, %.0f]%s\n", offset_keys[i],
                       visual->offsets.x[i], visual->offsets.y[i],
                       (printed < off_count) ? "," : "");
            }
        }
    }
    printf("}\n}\n");
}
