//
// Visual debug overlay implementation.
//

#include "debug_overlay.h"
#include "sprite_renderer.h"
#include "../core/debug_events.h"
#include "../core/battlefield.h"
#include "../entities/entity_animation.h"
#include "../logic/combat.h"
#include <math.h>

// --- Constants ---
#define BAR_WIDTH   30.0f
#define BAR_HEIGHT   4.0f
#define BAR_GAP      6.0f  // pixels above visible sprite top
#define HIT_FLASH_DURATION 0.15f

#define FLASH_RADIUS_MIN  5.0f
#define FLASH_RADIUS_MAX 20.0f

// --- Attack progress bar ---

static void draw_attack_bar(const Entity *e) {
    if (e->state != ESTATE_ATTACKING || !e->sprite) return;

    // Anchor bar above the visible sprite top
    Rectangle vb = sprite_visible_bounds(e->sprite, &e->anim,
                                         e->position, e->spriteScale,
                                         e->spriteRotationDegrees);
    float barX = e->position.x - BAR_WIDTH * 0.5f;
    float barY = (vb.height > 0.0f)
        ? vb.y - BAR_GAP - BAR_HEIGHT
        : e->position.y - 40.0f;

    // Background
    DrawRectangle((int)barX, (int)barY,
                  (int)BAR_WIDTH, (int)BAR_HEIGHT, DARKGRAY);

    // Fill — green→yellow→orange as progress advances
    float t = e->anim.normalizedTime;
    Color fillColor;
    if (t < 0.5f) {
        fillColor = GREEN;
    } else if (t < 0.8f) {
        fillColor = YELLOW;
    } else {
        fillColor = ORANGE;
    }

    // Flash white when hit marker was just crossed (event-driven via hitFlashTimer)
    if (e->hitFlashTimer > 0.0f) {
        fillColor = WHITE;
    }

    float fillWidth = t * BAR_WIDTH;
    if (fillWidth > 0.0f) {
        DrawRectangle((int)barX, (int)barY,
                      (int)fillWidth, (int)BAR_HEIGHT, fillColor);
    }

    // Hit marker tick — white vertical line at hitNormalized
    const EntityAnimSpec *spec = anim_spec_get(e->spriteType, ANIM_ATTACK);
    if (spec->hitNormalized >= 0.0f && spec->hitNormalized <= 1.0f) {
        float tickX = barX + spec->hitNormalized * BAR_WIDTH;
        DrawLineEx((Vector2){tickX, barY - 1.0f},
                   (Vector2){tickX, barY + BAR_HEIGHT + 1.0f},
                   1.5f, WHITE);
    }
}

// --- Target lock line ---

static void draw_target_line(const Entity *e, const Battlefield *bf,
                             const GameState *gs) {
    if (e->state != ESTATE_ATTACKING || e->attackTargetId < 0) return;

    Entity *target = bf_find_entity((Battlefield *)bf, e->attackTargetId);

    if (!target) {
        // Stale ID — red circle at attacker
        DrawCircleLinesV(e->position, 8.0f, RED);
        return;
    }

    Color lineColor;
    if (!target->alive) {
        lineColor = RED;
    } else if (!combat_in_range(e, target, gs)) {
        lineColor = YELLOW;
    } else {
        lineColor = GREEN;
    }

    DrawLineEx(e->position, target->position, 1.5f, lineColor);
}

// --- Attack range circles (moved from game.c) ---

static void draw_range_circles(const Battlefield *bf) {
    for (int i = 0; i < bf->entityCount; i++) {
        const Entity *e = bf->entities[i];
        if (!e || !e->alive) continue;

        Color c;
        switch (e->state) {
            case ESTATE_ATTACKING: c = RED;   break;
            case ESTATE_WALKING:   c = GREEN; break;
            case ESTATE_IDLE:      c = GRAY;  break;
            default:               continue;
        }

        DrawCircleLines((int)e->position.x, (int)e->position.y,
                        e->attackRange, c);
    }
}

// --- Event flashes ---

static void draw_event_flashes(void) {
    const DebugFlash *buf = debug_events_buffer();

    for (int i = 0; i < DEBUG_EVENT_CAPACITY; i++) {
        const DebugFlash *f = &buf[i];
        if (f->remaining <= 0.0f || f->duration <= 0.0f) continue;

        float progress = 1.0f - (f->remaining / f->duration);  // 0→1
        float radius = FLASH_RADIUS_MIN +
                       (FLASH_RADIUS_MAX - FLASH_RADIUS_MIN) * progress;
        float alpha = 1.0f - progress;  // fade out
        Vector2 pos = { f->posX, f->posY };

        Color baseColor;
        switch (f->type) {
            case DEBUG_EVT_STATE_CHANGE: baseColor = SKYBLUE; break;
            case DEBUG_EVT_HIT:          baseColor = RED;     break;
            case DEBUG_EVT_DEATH_FINISH: baseColor = PURPLE;  break;
            default:                     baseColor = WHITE;    break;
        }

        Color c = Fade(baseColor, alpha * 0.8f);
        DrawCircleLinesV(pos, radius, c);

        // Hit events also get a small filled circle
        if (f->type == DEBUG_EVT_HIT && alpha > 0.3f) {
            DrawCircleV(pos, 3.0f, Fade(RED, alpha * 0.6f));
        }
    }
}

// --- Public API ---

void debug_overlay_draw(const Battlefield *bf, const GameState *gs,
                        DebugOverlayFlags flags) {
    for (int i = 0; i < bf->entityCount; i++) {
        const Entity *e = bf->entities[i];
        if (!e || e->markedForRemoval) continue;

        if (flags.attackBars)   draw_attack_bar(e);
        if (flags.targetLines)  draw_target_line(e, bf, gs);
    }

    if (flags.eventFlashes)  draw_event_flashes();
    if (flags.rangeCirlces)  draw_range_circles(bf);
}
