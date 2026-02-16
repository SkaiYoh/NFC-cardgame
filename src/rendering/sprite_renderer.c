//
// Created by Nathan Davis on 2/16/26.
//

#include "sprite_renderer.h"
#include "../core/config.h"

// Helper: load one animation sheet and compute frame dimensions
static SpriteSheet load_sheet(const char *path, int frameCount) {
    SpriteSheet s = {0};
    s.texture = LoadTexture(path);
    SetTextureFilter(s.texture, TEXTURE_FILTER_POINT);
    s.frameCount = frameCount;
    s.frameWidth = s.texture.width / frameCount;
    s.frameHeight = s.texture.height / DIR_COUNT;
    return s;
}

void sprite_atlas_init(SpriteAtlas *atlas) {
    CharacterSprite *b = &atlas->base;
    b->anims[ANIM_IDLE]   = load_sheet(CHAR_BASE_PATH "Basic/idle.png",  4);
    b->anims[ANIM_RUN]    = load_sheet(CHAR_BASE_PATH "Basic/run.png",   8);
    b->anims[ANIM_WALK]   = load_sheet(CHAR_BASE_PATH "Basic/walk.png",  8);
    b->anims[ANIM_HURT]   = load_sheet(CHAR_BASE_PATH "Basic/hurt.png",  4);
    b->anims[ANIM_DEATH]  = load_sheet(CHAR_BASE_PATH "Basic/death.png", 6);
    b->anims[ANIM_ATTACK] = load_sheet(CHAR_BASE_PATH "Attack/sword.png", 6);
}

void sprite_atlas_free(SpriteAtlas *atlas) {
    CharacterSprite *b = &atlas->base;
    for (int i = 0; i < ANIM_COUNT; i++) {
        if (b->anims[i].texture.id > 0) {
            UnloadTexture(b->anims[i].texture);
        }
    }
}

void sprite_draw(const CharacterSprite *cs, const AnimState *state,
                 Vector2 pos, float scale) {
    const SpriteSheet *sheet = &cs->anims[state->anim];
    if (sheet->texture.id == 0) return;

    int col = state->frame % sheet->frameCount;
    int row = state->dir;

    float fw = (float)sheet->frameWidth;
    float fh = (float)sheet->frameHeight;

    // Flip source width negative for horizontal mirror
    Rectangle src = {
        (float)(col * sheet->frameWidth),
        (float)(row * sheet->frameHeight),
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
    Vector2 origin = { dw / 2.0f, dh / 2.0f };

    DrawTexturePro(sheet->texture, src, dst, origin, 0.0f, WHITE);
}

void anim_state_init(AnimState *state, AnimationType anim, SpriteDirection dir, float fps) {
    state->anim = anim;
    state->dir = dir;
    state->frame = 0;
    state->timer = 0.0f;
    state->fps = fps;
    state->flipH = false;
}

void anim_state_update(AnimState *state, float dt) {
    const float frameDuration = 1.0f / state->fps;
    state->timer += dt;
    while (state->timer >= frameDuration) {
        state->timer -= frameDuration;
        state->frame++;
    }
    // Frame wrapping is handled in sprite_draw via modulo
}
