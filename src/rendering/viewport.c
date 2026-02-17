//
// Created by Nathan Davis on 2/16/26.
//

#include "viewport.h"
#include "../systems/player.h"
#include <stdio.h>

void viewport_init_split_screen(GameState *gs) {
    gs->halfWidth = SCREEN_WIDTH / 2;

    // Player 1: Left half of screen, rotated 90 degrees
    Rectangle p1PlayArea = {
        .x = 0,
        .y = 0,
        .width = SCREEN_HEIGHT,
        .height = gs->halfWidth
    };
    Rectangle p1ScreenArea = {
        .x = 0,
        .y = 0,
        .width = gs->halfWidth,
        .height = SCREEN_HEIGHT
    };

    // Player 2: Right half of screen, rotated -90 degrees
    Rectangle p2PlayArea = {
        .x = gs->halfWidth,
        .y = 0,
        .width = SCREEN_HEIGHT,
        .height = gs->halfWidth
    };
    Rectangle p2ScreenArea = {
        .x = gs->halfWidth,
        .y = 0,
        .width = gs->halfWidth,
        .height = SCREEN_HEIGHT
    };

    float tileSize = DEFAULT_TILE_SIZE * DEFAULT_TILE_SCALE;

    // Initialize players with their biome definitions
    player_init(&gs->players[0], 0, p1PlayArea, p1ScreenArea, 90.0f,
                BIOME_GRASS, &gs->biomeDefs[BIOME_GRASS], tileSize, 42);
    player_init(&gs->players[1], 1, p2PlayArea, p2ScreenArea, -90.0f,
                BIOME_UNDEAD, &gs->biomeDefs[BIOME_UNDEAD], tileSize, 99);

    printf("Split-screen viewports initialized\n");
}

void viewport_begin(Player *p) {
    BeginScissorMode(
        (int)p->screenArea.x,
        (int)p->screenArea.y,
        (int)p->screenArea.width,
        (int)p->screenArea.height
    );
    BeginMode2D(p->camera);
}

void viewport_end(void) {
    EndMode2D();
    EndScissorMode();
}

Vector2 viewport_world_to_screen(Player *p, Vector2 worldPos) {
    return GetWorldToScreen2D(worldPos, p->camera);
}

Vector2 viewport_screen_to_world(Player *p, Vector2 screenPos) {
    return GetScreenToWorld2D(screenPos, p->camera);
}

void viewport_draw_tilemap(Player *p) {
    tilemap_draw(&p->tilemap, p->tileDefs);
    tilemap_draw_details(&p->tilemap, p->detailDefs);
}

void viewport_draw_card_slots_debug(Player *p) {
    // Draw debug circles at card slot positions
    for (int i = 0; i < NUM_CARD_SLOTS; i++) {
        CardSlot *slot = &p->slots[i];
        Color slotColor = slot->isOccupied ? RED : GREEN;

        // Draw slot indicator
        DrawCircleV(slot->worldPos, 20.0f, slotColor);
        DrawCircleLines(slot->worldPos.x, slot->worldPos.y, 25.0f, WHITE);

        // Draw slot number
        char slotNum[2];
        slotNum[0] = '1' + i;
        slotNum[1] = '\0';
        DrawText(slotNum, slot->worldPos.x - 5, slot->worldPos.y - 8, 16, WHITE);
    }
}
