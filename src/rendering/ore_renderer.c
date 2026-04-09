//
// Ore node renderer implementation.
//
// TODO: Single ore texture for all types. Add type-based texture selection
// when ore variety is implemented (keyed on OreNode.oreType).

#include "ore_renderer.h"
#include "../core/config.h"

#define ORE_TEXTURE_PATH "src/assets/environment/Objects/uvulite_blob.png"
#define ORE_FRAME_COUNT 2
#define ORE_ANIM_FPS 2.0

// Draw scale: ore source is ~32px, grid cell is 64px.
#define ORE_RENDER_SCALE 2.0f

Texture2D ore_renderer_load(void) {
    return LoadTexture(ORE_TEXTURE_PATH);
}

static float ore_anim_phase_seconds(const OreNode *node) {
    unsigned int hash = (unsigned int) (node->id + 1) * 2654435761u;
    float cycleDuration = (float) ORE_FRAME_COUNT / (float) ORE_ANIM_FPS;
    return ((float) (hash & 0xFFFFu) / 65535.0f) * cycleDuration;
}

void ore_renderer_draw(const OreField *field, BattleSide side, Texture2D texture,
                       float rotationDegrees) {
    if (texture.id == 0) return;

    float frameWidth = (float) texture.width / (float) ORE_FRAME_COUNT;
    float frameHeight = (float) texture.height;

    for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
        const OreNode *n = &field->nodes[side][i];
        if (!n->active) continue;

        float animTime = (float) GetTime() + ore_anim_phase_seconds(n);
        int frame = ((int) (animTime * (float) ORE_ANIM_FPS)) % ORE_FRAME_COUNT;
        float drawW = frameWidth * ORE_RENDER_SCALE;
        float drawH = frameHeight * ORE_RENDER_SCALE;

        Rectangle src = {
            frameWidth * (float) frame,
            0.0f,
            frameWidth,
            frameHeight
        };
        Rectangle dst = {
            n->worldPos.v.x,
            n->worldPos.v.y,
            drawW,
            drawH
        };
        DrawTexturePro(texture, src, dst, (Vector2){drawW * 0.5f, drawH * 0.5f},
                       rotationDegrees, WHITE);
    }
}
