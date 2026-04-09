//
// World-anchored, screen-aligned status bars for troops and bases.
//

#include "status_bars.h"
#include "sprite_renderer.h"
#include "../core/config.h"
#include <math.h>
#include <stdio.h>

#define STATUS_BAR_ROW_HEIGHT        16.0f
#define STATUS_BAR_CELL_STRIDE      144.0f
#define STATUS_BAR_FRAME_X_OFFSET     3.0f
#define STATUS_BAR_TROOP_WIDTH       42.0f
#define STATUS_BAR_BASE_WIDTH       138.0f
#define STATUS_BAR_TROOP_FRAMES      20
#define STATUS_BAR_BASE_FRAMES       24
#define STATUS_BAR_TROOP_ROW_Y        0.0f
#define STATUS_BAR_BASE_HEALTH_ROW_Y 16.0f
#define STATUS_BAR_BASE_ENERGY_ROW_Y 32.0f
#define STATUS_BAR_TOP_GAP            6.0f
#define STATUS_BAR_STACK_GAP          4.0f

static float clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static Rectangle status_bar_src(int frameIndex, float rowY, float width) {
    return (Rectangle){
        STATUS_BAR_FRAME_X_OFFSET + STATUS_BAR_CELL_STRIDE * (float)frameIndex,
        rowY,
        width,
        STATUS_BAR_ROW_HEIGHT
    };
}

static int troop_health_frame(const Entity *troop) {
    if (!troop || troop->hp <= 0 || troop->maxHP <= 0) return -1;

    float ratio = clamp01((float)troop->hp / (float)troop->maxHP);
    if (ratio >= 1.0f) return 0;

    int frame = (int)floorf((1.0f - ratio) * (float)STATUS_BAR_TROOP_FRAMES);
    if (frame < 0) frame = 0;
    if (frame >= STATUS_BAR_TROOP_FRAMES) frame = STATUS_BAR_TROOP_FRAMES - 1;
    return frame;
}

static int base_stat_frame(float value, float maxValue) {
    if (value <= 0.0f || maxValue <= 0.0f) return STATUS_BAR_BASE_FRAMES - 1;

    float ratio = clamp01(value / maxValue);
    if (ratio >= 1.0f) return 0;

    int frame = (int)floorf((1.0f - ratio) * (float)(STATUS_BAR_BASE_FRAMES - 1));
    if (frame < 0) frame = 0;
    if (frame >= STATUS_BAR_BASE_FRAMES - 1) frame = STATUS_BAR_BASE_FRAMES - 2;
    return frame;
}

static Rectangle entity_visible_world_bounds(const Entity *e) {
    if (!e) return (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};

    if (e->sprite) {
        Rectangle visible = sprite_visible_bounds(e->sprite, &e->anim,
                                                  e->position, e->spriteScale,
                                                  e->spriteRotationDegrees);
        if (visible.width > 0.0f && visible.height > 0.0f) {
            return visible;
        }

        const SpriteSheet *sheet = sprite_sheet_get(e->sprite, e->anim.anim);
        if (sheet && sheet->frameWidth > 0 && sheet->frameHeight > 0) {
            float width = (float)sheet->frameWidth * e->spriteScale;
            float height = (float)sheet->frameHeight * e->spriteScale;
            return (Rectangle){
                e->position.x - width * 0.5f,
                e->position.y - height * 0.5f,
                width,
                height
            };
        }
    }

    return (Rectangle){e->position.x, e->position.y, 0.0f, 0.0f};
}

static Rectangle entity_stable_visible_world_bounds(const Entity *e) {
    if (!e || !e->sprite) {
        return entity_visible_world_bounds(e);
    }

    const SpriteSheet *sheet = sprite_sheet_get(e->sprite, e->anim.anim);
    if (!sheet || sheet->frameWidth <= 0 || sheet->frameHeight <= 0) {
        return entity_visible_world_bounds(e);
    }

    float rad = e->spriteRotationDegrees * (PI_F / 180.0f);
    float cosA = cosf(rad);
    float sinA = sinf(rad);
    float fw = (float)sheet->frameWidth * e->spriteScale;
    float fh = (float)sheet->frameHeight * e->spriteScale;
    float cx = fw * 0.5f;
    float cy = fh * 0.5f;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    bool foundVisible = false;

    for (int dir = 0; dir < DIR_COUNT; dir++) {
        for (int frame = 0; frame < sheet->frameCount; frame++) {
            Rectangle bounds;
            if (sheet->visibleBounds) {
                bounds = sheet->visibleBounds[dir * sheet->frameCount + frame];
                if (bounds.width <= 0.0f || bounds.height <= 0.0f) continue;
            } else {
                bounds = (Rectangle){0.0f, 0.0f, (float)sheet->frameWidth, (float)sheet->frameHeight};
            }

            float vx = bounds.x * e->spriteScale;
            float vy = bounds.y * e->spriteScale;
            float vw = bounds.width * e->spriteScale;
            float vh = bounds.height * e->spriteScale;
            if (e->anim.flipH) {
                vx = fw - (vx + vw);
            }

            float corners[4][2] = {
                { vx - cx,      vy - cy },
                { vx + vw - cx, vy - cy },
                { vx + vw - cx, vy + vh - cy },
                { vx - cx,      vy + vh - cy },
            };

            for (int i = 0; i < 4; i++) {
                float rx = corners[i][0] * cosA - corners[i][1] * sinA;
                float ry = corners[i][0] * sinA + corners[i][1] * cosA;
                float wx = e->position.x + rx;
                float wy = e->position.y + ry;
                if (!foundVisible || wx < minX) minX = wx;
                if (!foundVisible || wy < minY) minY = wy;
                if (!foundVisible || wx > maxX) maxX = wx;
                if (!foundVisible || wy > maxY) maxY = wy;
                foundVisible = true;
            }
        }
    }

    if (!foundVisible) {
        return entity_visible_world_bounds(e);
    }

    return (Rectangle){ minX, minY, maxX - minX, maxY - minY };
}

static Rectangle world_rect_to_screen(Rectangle worldRect, Camera2D camera) {
    Vector2 corners[4] = {
        {worldRect.x, worldRect.y},
        {worldRect.x + worldRect.width, worldRect.y},
        {worldRect.x + worldRect.width, worldRect.y + worldRect.height},
        {worldRect.x, worldRect.y + worldRect.height}
    };

    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    for (int i = 0; i < 4; i++) {
        Vector2 screen = GetWorldToScreen2D(corners[i], camera);
        if (i == 0 || screen.x < minX) minX = screen.x;
        if (i == 0 || screen.y < minY) minY = screen.y;
        if (i == 0 || screen.x > maxX) maxX = screen.x;
        if (i == 0 || screen.y > maxY) maxY = screen.y;
    }

    return (Rectangle){minX, minY, maxX - minX, maxY - minY};
}

static Vector2 entity_screen_head_direction(const Entity *e, Camera2D camera) {
    if (!e) return (Vector2){0.0f, -1.0f};

    float rad = e->spriteRotationDegrees * (PI_F / 180.0f);
    Vector2 worldHeadDir = { sinf(rad), -cosf(rad) };
    Vector2 center = GetWorldToScreen2D(e->position, camera);
    Vector2 head = GetWorldToScreen2D(
        (Vector2){
            e->position.x + worldHeadDir.x * 32.0f,
            e->position.y + worldHeadDir.y * 32.0f
        },
        camera
    );
    Vector2 dir = { head.x - center.x, head.y - center.y };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (len <= 0.001f) {
        return (Vector2){0.0f, -1.0f};
    }
    return (Vector2){ dir.x / len, dir.y / len };
}

static Vector2 screen_anchor_from_bounds(Rectangle screenBounds, const Entity *e,
                                         Camera2D camera, Vector2 headDirection) {
    Vector2 center = {
        screenBounds.x + screenBounds.width * 0.5f,
        screenBounds.y + screenBounds.height * 0.5f
    };

    if (screenBounds.width <= 0.0f || screenBounds.height <= 0.0f) {
        Vector2 fallback = GetWorldToScreen2D(e->position, camera);
        float fallbackGap = 40.0f;
        return (Vector2){
            fallback.x + headDirection.x * fallbackGap,
            fallback.y + headDirection.y * fallbackGap
        };
    }

    if (fabsf(headDirection.x) >= fabsf(headDirection.y)) {
        center.x = (headDirection.x >= 0.0f)
            ? (screenBounds.x + screenBounds.width)
            : screenBounds.x;
    } else {
        center.y = (headDirection.y >= 0.0f)
            ? (screenBounds.y + screenBounds.height)
            : screenBounds.y;
    }

    return center;
}

static Vector2 entity_screen_anchor(const Entity *e, Camera2D camera, Vector2 headDirection) {
    Rectangle screenBounds = world_rect_to_screen(entity_visible_world_bounds(e), camera);
    return screen_anchor_from_bounds(screenBounds, e, camera, headDirection);
}

static void draw_status_bar(Texture2D texture, Rectangle src, Vector2 screenCenter,
                            float rotationDegrees) {
    if (texture.id == 0) return;

    Rectangle dst = {
        screenCenter.x,
        screenCenter.y,
        src.width,
        src.height
    };
    Vector2 origin = { dst.width * 0.5f, dst.height * 0.5f };

    DrawTexturePro(texture, src, dst, origin, rotationDegrees, WHITE);
}

static void draw_troop_health_bar(const GameState *gs, const Entity *troop,
                                  Camera2D camera,
                                  float rotationDegrees) {
    int frame = troop_health_frame(troop);
    if (frame < 0) return;

    Vector2 headDirection = entity_screen_head_direction(troop, camera);
    Rectangle troopScreenBounds = world_rect_to_screen(
        entity_stable_visible_world_bounds(troop), camera
    );
    Vector2 anchor = screen_anchor_from_bounds(
        troopScreenBounds, troop, camera, headDirection
    );
    Rectangle src = status_bar_src(frame, STATUS_BAR_TROOP_ROW_Y, STATUS_BAR_TROOP_WIDTH);
    Vector2 center = {
        anchor.x + headDirection.x * (STATUS_BAR_TOP_GAP + src.height * 0.5f),
        anchor.y + headDirection.y * (STATUS_BAR_TOP_GAP + src.height * 0.5f)
    };

    draw_status_bar(gs->statusBarsTexture, src, center, rotationDegrees);
}

static void draw_base_bars(const GameState *gs, const Entity *base, const Player *owner,
                           Camera2D camera,
                           float rotationDegrees) {
    if (!base || !owner) return;

    Vector2 headDirection = entity_screen_head_direction(base, camera);
    Rectangle baseScreenBounds = world_rect_to_screen(
        entity_stable_visible_world_bounds(base), camera
    );
    Vector2 anchor = {
        baseScreenBounds.x + baseScreenBounds.width * 0.5f,
        baseScreenBounds.y + baseScreenBounds.height * 0.5f
    };
    if (baseScreenBounds.width <= 0.0f || baseScreenBounds.height <= 0.0f) {
        anchor = entity_screen_anchor(base, camera, headDirection);
    } else {
        anchor = screen_anchor_from_bounds(baseScreenBounds, base, camera, headDirection);
    }

    Rectangle healthSrc = status_bar_src(
        base_stat_frame((float)base->hp, (float)base->maxHP),
        STATUS_BAR_BASE_HEALTH_ROW_Y,
        STATUS_BAR_BASE_WIDTH
    );
    Rectangle energySrc = status_bar_src(
        base_stat_frame(owner->energy, owner->maxEnergy),
        STATUS_BAR_BASE_ENERGY_ROW_Y,
        STATUS_BAR_BASE_WIDTH
    );

    float firstOffset = STATUS_BAR_TOP_GAP + healthSrc.height * 0.5f;
    float secondOffset = STATUS_BAR_TOP_GAP + healthSrc.height +
                         STATUS_BAR_STACK_GAP + energySrc.height * 0.5f;
    Vector2 healthCenter = {
        anchor.x + headDirection.x * firstOffset,
        anchor.y + headDirection.y * firstOffset
    };
    Vector2 energyCenter = {
        anchor.x + headDirection.x * secondOffset,
        anchor.y + headDirection.y * secondOffset
    };

    draw_status_bar(gs->statusBarsTexture, healthSrc, healthCenter, rotationDegrees);
    draw_status_bar(gs->statusBarsTexture, energySrc, energyCenter, rotationDegrees);
}

Texture2D status_bars_load(void) {
    Texture2D texture = LoadTexture(STATUS_BARS_PATH);
    if (texture.id == 0) {
        printf("[STATUS_BARS] Failed to load %s\n", STATUS_BARS_PATH);
        return texture;
    }

    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return texture;
}

void status_bars_unload(Texture2D texture) {
    if (texture.id > 0) {
        UnloadTexture(texture);
    }
}

void status_bars_draw_screen(const GameState *gs, Camera2D camera,
                             float rotationDegrees) {
    if (!gs || gs->statusBarsTexture.id == 0) return;

    const Battlefield *bf = &gs->battlefield;
    for (int i = 0; i < bf->entityCount; i++) {
        const Entity *e = bf->entities[i];
        if (!e || e->markedForRemoval || e->type != ENTITY_TROOP) continue;
        if (!e->alive || e->hp <= 0) continue;
        draw_troop_health_bar(gs, e, camera, rotationDegrees);
    }

    for (int i = 0; i < 2; i++) {
        const Player *player = &gs->players[i];
        const Entity *base = player->base;
        if (!base || base->markedForRemoval) continue;
        draw_base_bars(gs, base, player, camera, rotationDegrees);
    }
}
