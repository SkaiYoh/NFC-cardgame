//
// Created by Nathan Davis on 2/16/26.
//

#include "sprite_renderer.h"
#include "../core/config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
    const char *path;
    int frameCount;
    int sourceRowCount;
    const Rectangle *visibleBounds;
} SpriteSheetAtlasEntry;

typedef struct {
    bool isBaseFallback;
    SpriteType spriteType;
    AnimationType anim;
    const char *path;
    int frameCount;
    int sourceRowCount;
    bool required;
} SpriteSheetManifestEntry;

#include "sprite_frame_atlas.h"

static const SpriteSheetManifestEntry kSpriteSheetManifest[] = {
#define SPRITE_SHEET(name, isBaseFallback, spriteType, anim, path, frameCount, sourceRowCount, required) \
    { isBaseFallback, spriteType, anim, path, frameCount, sourceRowCount, required },
#include "sprite_sheet_manifest.def"
#undef SPRITE_SHEET
};

static const int kSpriteSheetManifestCount =
    (int) (sizeof(kSpriteSheetManifest) / sizeof(kSpriteSheetManifest[0]));

static int sheet_bounds_index(const SpriteSheet *sheet, SpriteDirection dir, int frame) {
    return dir * sheet->frameCount + frame;
}

static bool sheet_has_content(const SpriteSheet *sheet) {
    if (!sheet) return false;
    return sheet->texture.id != 0 ||
           sheet->frameCount > 0 ||
           sheet->frameWidth > 0 ||
           sheet->frameHeight > 0 ||
           sheet->visibleBounds != NULL;
}

static int sheet_source_row(const SpriteSheet *sheet, SpriteDirection dir) {
    if (!sheet) return 0;

    int sourceRowCount = (sheet->sourceRowCount > 0) ? sheet->sourceRowCount : DIR_COUNT;
    if (sourceRowCount <= 1) return 0;

    int row = (int) dir;
    if (row < 0) return 0;
    if (row >= sourceRowCount) return sourceRowCount - 1;
    return row;
}

static int anim_visual_loops(const AnimState *state) {
    if (!state || state->visualLoops < 1) return 1;
    return state->visualLoops;
}

static int anim_frame_index(const AnimState *state, const SpriteSheet *sheet) {
    if (!sheet || sheet->frameCount <= 0) return 0;

    float normalized = state ? state->normalizedTime : 0.0f;
    if (normalized < 0.0f) normalized = 0.0f;

    if (state && state->oneShot && normalized >= 1.0f) {
        return sheet->frameCount - 1;
    }

    float phase = normalized * (float)anim_visual_loops(state);
    float localPhase = phase - floorf(phase);
    int frame = (int)(localPhase * (float)sheet->frameCount);
    if (frame >= sheet->frameCount) frame = sheet->frameCount - 1;
    if (frame < 0) frame = 0;
    return frame;
}

static Rectangle compute_visible_bounds(const Color *pixels, int imageWidth,
                                        int frameX, int frameY,
                                        int frameWidth, int frameHeight) {
    int minX = frameWidth;
    int minY = frameHeight;
    int maxX = -1;
    int maxY = -1;

    for (int y = 0; y < frameHeight; y++) {
        for (int x = 0; x < frameWidth; x++) {
            Color px = pixels[(frameY + y) * imageWidth + (frameX + x)];
            if (px.a == 0) continue;

            if (x < minX) minX = x;
            if (y < minY) minY = y;
            if (x > maxX) maxX = x;
            if (y > maxY) maxY = y;
        }
    }

    if (maxX < minX || maxY < minY) {
        return (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
    }

    return (Rectangle){
        (float) minX,
        (float) minY,
        (float) (maxX - minX + 1),
        (float) (maxY - minY + 1)
    };
}

static const SpriteSheetAtlasEntry *find_sheet_atlas_entry(const char *path, int frameCount,
                                                           int sourceRowCount) {
    for (int i = 0; i < kSpriteSheetAtlasCount; i++) {
        const SpriteSheetAtlasEntry *entry = &kSpriteSheetAtlas[i];
        if (entry->frameCount == frameCount &&
            entry->sourceRowCount == sourceRowCount &&
            strcmp(entry->path, path) == 0) {
            return entry;
        }
    }
    return NULL;
}

static Rectangle *alloc_visible_bounds(int frameCount) {
    return calloc((size_t) (frameCount * DIR_COUNT), sizeof(Rectangle));
}

static void populate_visible_bounds_from_image(SpriteSheet *sheet, const char *path) {
    if (!sheet->visibleBounds) return;

    Image image = LoadImage(path);
    if (!image.data) return;

    Color *pixels = LoadImageColors(image);
    if (pixels) {
        for (int dir = 0; dir < DIR_COUNT; dir++) {
            for (int frame = 0; frame < sheet->frameCount; frame++) {
                int frameX = frame * sheet->frameWidth;
                int frameY = sheet_source_row(sheet, (SpriteDirection) dir) * sheet->frameHeight;
                sheet->visibleBounds[sheet_bounds_index(sheet, (SpriteDirection) dir, frame)] =
                    compute_visible_bounds(pixels, image.width, frameX, frameY,
                                           sheet->frameWidth, sheet->frameHeight);
            }
        }
        UnloadImageColors(pixels);
    }

    UnloadImage(image);
}

// Helper: load one animation sheet and compute frame dimensions
static SpriteSheet load_sheet_with_rows(const char *path, int frameCount, int sourceRowCount,
                                        bool required) {
    SpriteSheet s = {0};
    const char *sheetKind = required ? "required" : "optional";

    if (!path || frameCount < 1 || sourceRowCount < 1) {
        fprintf(stderr,
                "[sprite] Invalid %s sheet manifest entry: path=%s frames=%d rows=%d\n",
                sheetKind, path ? path : "(null)", frameCount, sourceRowCount);
        return s;
    }

    s.texture = LoadTexture(path);
    if (s.texture.id == 0) {
        fprintf(stderr, "[sprite] Failed to load %s sheet: %s\n", sheetKind, path);
        return s;
    }

    SetTextureFilter(s.texture, TEXTURE_FILTER_POINT);

    if (s.texture.width <= 0 || s.texture.height <= 0 ||
        s.texture.width % frameCount != 0 ||
        s.texture.height % sourceRowCount != 0) {
        fprintf(stderr,
                "[sprite] Invalid %s sheet dimensions for %s "
                "(got %dx%d, frames=%d, rows=%d)\n",
                sheetKind, path, s.texture.width, s.texture.height, frameCount, sourceRowCount);
        UnloadTexture(s.texture);
        return (SpriteSheet){0};
    }

    s.frameCount = frameCount;
    s.frameWidth = s.texture.width / frameCount;
    s.sourceRowCount = sourceRowCount;
    s.frameHeight = s.texture.height / s.sourceRowCount;
    s.visibleBounds = alloc_visible_bounds(s.frameCount);

    const SpriteSheetAtlasEntry *entry = find_sheet_atlas_entry(path, frameCount, sourceRowCount);
    if (s.visibleBounds && entry) {
        memcpy(s.visibleBounds, entry->visibleBounds,
               (size_t) (s.frameCount * DIR_COUNT) * sizeof(Rectangle));
    } else {
        populate_visible_bounds_from_image(&s, path);
    }

    return s;
}

static SpriteSheet load_sheet_manifest(const SpriteSheetManifestEntry *entry) {
    if (!entry) return (SpriteSheet){0};
    return load_sheet_with_rows(entry->path, entry->frameCount, entry->sourceRowCount,
                                entry->required);
}

void sprite_atlas_init(SpriteAtlas *atlas) {
    if (!atlas) return;
    memset(atlas, 0, sizeof(*atlas));

    for (int i = 0; i < kSpriteSheetManifestCount; i++) {
        const SpriteSheetManifestEntry *entry = &kSpriteSheetManifest[i];
        CharacterSprite *target = entry->isBaseFallback
            ? &atlas->base
            : &atlas->types[entry->spriteType];
        target->anims[entry->anim] = load_sheet_manifest(entry);

        if (!entry->isBaseFallback &&
            entry->spriteType >= 0 &&
            entry->spriteType < SPRITE_TYPE_COUNT) {
            atlas->typeLoaded[entry->spriteType] = true;
        }
    }
}

void sprite_atlas_free(SpriteAtlas *atlas) {
    // Free base sprites
    CharacterSprite *b = &atlas->base;
    for (int i = 0; i < ANIM_COUNT; i++) {
        free(b->anims[i].visibleBounds);
        b->anims[i].visibleBounds = NULL;
        if (b->anims[i].texture.id > 0) {
            UnloadTexture(b->anims[i].texture);
        }
    }

    // Free per-type sprites
    for (int t = 0; t < SPRITE_TYPE_COUNT; t++) {
        if (!atlas->typeLoaded[t]) continue;
        for (int i = 0; i < ANIM_COUNT; i++) {
            free(atlas->types[t].anims[i].visibleBounds);
            atlas->types[t].anims[i].visibleBounds = NULL;
            if (atlas->types[t].anims[i].texture.id > 0) {
                UnloadTexture(atlas->types[t].anims[i].texture);
            }
        }
    }
}

const SpriteSheet *sprite_sheet_get(const CharacterSprite *cs, AnimationType anim) {
    if (!cs || anim < 0 || anim >= ANIM_COUNT) return NULL;

    static const AnimationType kFallbacks[ANIM_COUNT][3] = {
        [ANIM_IDLE] = { ANIM_IDLE, ANIM_WALK, ANIM_RUN },
        [ANIM_RUN] = { ANIM_RUN, ANIM_WALK, ANIM_IDLE },
        [ANIM_WALK] = { ANIM_WALK, ANIM_RUN, ANIM_IDLE },
        [ANIM_HURT] = { ANIM_HURT, ANIM_DEATH, ANIM_HURT },
        [ANIM_DEATH] = { ANIM_DEATH, ANIM_HURT, ANIM_DEATH },
        [ANIM_ATTACK] = { ANIM_ATTACK, ANIM_ATTACK, ANIM_ATTACK },
    };

    for (int i = 0; i < 3; i++) {
        AnimationType candidate = kFallbacks[anim][i];
        const SpriteSheet *sheet = &cs->anims[candidate];
        if (sheet_has_content(sheet)) {
            return sheet;
        }
    }

    return &cs->anims[anim];
}

Rectangle sprite_visible_bounds(const CharacterSprite *cs, const AnimState *state,
                                Vector2 pos, float scale, float rotationDegrees) {
    if (!state) return (Rectangle){pos.x, pos.y, 0.0f, 0.0f};

    const SpriteSheet *sheet = sprite_sheet_get(cs, state->anim);
    if (!sheet) return (Rectangle){pos.x, pos.y, 0.0f, 0.0f};

    int frame = anim_frame_index(state, sheet);
    Rectangle bounds = {0.0f, 0.0f, (float) sheet->frameWidth, (float) sheet->frameHeight};
    if (sheet->visibleBounds) {
        bounds = sheet->visibleBounds[sheet_bounds_index(sheet, state->dir, frame)];
    }
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return (Rectangle){pos.x, pos.y, 0.0f, 0.0f};
    }

    float fw = (float) sheet->frameWidth * scale;
    float fh = (float) sheet->frameHeight * scale;

    // Visible sub-rect in scaled frame-local coords (origin = frame top-left)
    float vx = bounds.x * scale;
    float vy = bounds.y * scale;
    float vw = bounds.width * scale;
    float vh = bounds.height * scale;

    // Apply flipH: mirror visible rect within the full frame
    if (state->flipH) {
        vx = fw - (vx + vw);
    }

    // Four corners relative to the frame center (same origin as DrawTexturePro)
    float cx = fw * 0.5f;
    float cy = fh * 0.5f;
    float corners[4][2] = {
        { vx - cx,      vy - cy },
        { vx + vw - cx, vy - cy },
        { vx + vw - cx, vy + vh - cy },
        { vx - cx,      vy + vh - cy },
    };

    // Rotate corners around the frame center
    float rad = rotationDegrees * (3.14159265358979323846f / 180.0f);
    float cosA = cosf(rad);
    float sinA = sinf(rad);

    float minX = 1e30f, minY = 1e30f;
    float maxX = -1e30f, maxY = -1e30f;
    for (int i = 0; i < 4; i++) {
        float rx = corners[i][0] * cosA - corners[i][1] * sinA;
        float ry = corners[i][0] * sinA + corners[i][1] * cosA;
        // Translate back to world space
        float wx = pos.x + rx;
        float wy = pos.y + ry;
        if (wx < minX) minX = wx;
        if (wy < minY) minY = wy;
        if (wx > maxX) maxX = wx;
        if (wy > maxY) maxY = wy;
    }

    return (Rectangle){ minX, minY, maxX - minX, maxY - minY };
}

void sprite_draw(const CharacterSprite *cs, const AnimState *state,
                 Vector2 pos, float scale, float rotationDegrees) {
    const SpriteSheet *sheet = sprite_sheet_get(cs, state->anim);
    // TODO: When LoadTexture fails, texture.id == 0 and we silently return without drawing.
    // TODO: This is safe but gives no indication of why nothing appears. Log a warning at load time.
    if (!sheet || sheet->texture.id == 0) return;

    int col = anim_frame_index(state, sheet);
    int row = sheet_source_row(sheet, state->dir);

    float fw = (float) sheet->frameWidth;
    float fh = (float) sheet->frameHeight;

    // Flip source width negative for horizontal mirror
    Rectangle src = {
        (float) (col * sheet->frameWidth),
        (float) (row * sheet->frameHeight),
        state->flipH ? -fw : fw,
        fh
    };

    float dw = fw * scale;
    float dh = fh * scale;

    Rectangle dst = {
        pos.x,
        pos.y,
        dw,
        dh
    };

    // Center the sprite on the position
    Vector2 origin = {dw / 2.0f, dh / 2.0f};

    DrawTexturePro(sheet->texture, src, dst, origin, rotationDegrees, WHITE);
}


void anim_state_init(AnimState *state, AnimationType anim, SpriteDirection dir,
                     float cycleDuration, bool oneShot) {
    anim_state_init_with_loops(state, anim, dir, cycleDuration, oneShot, 1);
}

void anim_state_init_with_loops(AnimState *state, AnimationType anim, SpriteDirection dir,
                                float cycleDuration, bool oneShot, int visualLoops) {
    state->anim = anim;
    state->dir = dir;
    state->elapsed = 0.0f;
    state->cycleDuration = cycleDuration;
    state->normalizedTime = 0.0f;
    state->oneShot = oneShot;
    state->finished = false;
    state->flipH = false;
    state->visualLoops = (visualLoops > 0) ? visualLoops : 1;
}

AnimPlaybackEvent anim_state_update(AnimState *state, float dt) {
    AnimPlaybackEvent evt = {0};

    if (state->cycleDuration <= 0.0f) {
        // Degenerate clip: stay at frame 0, report finished immediately for one-shot
        evt.finishedThisTick = state->oneShot && !state->finished;
        state->finished = state->oneShot;
        return evt;
    }

    evt.prevNormalized = state->normalizedTime;
    state->elapsed += dt;

    if (state->oneShot) {
        if (state->elapsed >= state->cycleDuration) {
            state->elapsed = state->cycleDuration;
            state->normalizedTime = 1.0f;
            if (!state->finished) {
                evt.finishedThisTick = true;
                state->finished = true;
            }
        } else {
            state->normalizedTime = state->elapsed / state->cycleDuration;
        }
    } else {
        // Looping: wrap elapsed and compute normalized [0, 1)
        while (state->elapsed >= state->cycleDuration) {
            state->elapsed -= state->cycleDuration;
            evt.loopedThisTick = true;
        }
        state->normalizedTime = state->elapsed / state->cycleDuration;
    }

    evt.currNormalized = state->normalizedTime;
    return evt;
}

const CharacterSprite *sprite_atlas_get(const SpriteAtlas *atlas, SpriteType type) {
    if (type >= 0 && type < SPRITE_TYPE_COUNT && atlas->typeLoaded[type]) {
        return &atlas->types[type];
    }
    return &atlas->base;
}

SpriteType sprite_type_from_card(const char *cardType) {
    if (!cardType) return SPRITE_TYPE_KNIGHT;
    if (strcmp(cardType, "knight") == 0) return SPRITE_TYPE_KNIGHT;
    if (strcmp(cardType, "healer") == 0) return SPRITE_TYPE_HEALER;
    if (strcmp(cardType, "assassin") == 0) return SPRITE_TYPE_ASSASSIN;
    if (strcmp(cardType, "brute") == 0) return SPRITE_TYPE_BRUTE;
    if (strcmp(cardType, "farmer") == 0) return SPRITE_TYPE_FARMER;
    if (strcmp(cardType, "bird") == 0) return SPRITE_TYPE_BIRD;
    if (strcmp(cardType, "fishfing") == 0) return SPRITE_TYPE_FISHFING;
    fprintf(stderr, "[sprite] unknown card type '%s', falling back to KNIGHT\n", cardType);
    return SPRITE_TYPE_KNIGHT;
}
