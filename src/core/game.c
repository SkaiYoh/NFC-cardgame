//
// Created by Nathan Davis on 2/16/26.
//

#include "game.h"
#include "../logic/card_effects.h"
#include "../rendering/viewport.h"
#include "../systems/player.h"
#include <stdio.h>
#include <stdlib.h>

bool game_init(GameState *g) {
    const char *conninfo = getenv("DB_CONNECTION");
    if (!db_init(&g->db, conninfo)) {
        return false;
    }

    if (!cards_load(&g->deck, &g->db)) {
        db_close(&g->db);
        return false;
    }

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "NFC Card Game");
    SetTargetFPS(60);

    // Initialize card system
    card_action_init();
    card_atlas_init(&g->cardAtlas);

    // DEBUG: set test visual
    g->testVisual = card_visual_default();
    Card *testCard = cards_find(&g->deck, "FARMER_001");
    if (testCard && testCard->data) {
        // DEBUG
        // printf("Card data from DB: %s\n", testCard->data);
        g->testVisual = card_visual_from_json(testCard->data);
    }


    // test play every card in the deck
    for (int i = 0; i < g->deck.count; i++) {
        card_action_play(&g->deck.cards[i], NULL);
    }

    // Initialize shared tileset
    g->tilesetTex = LoadTexture(GRASS_TILESET_PATH);
    SetTextureFilter(g->tilesetTex, TEXTURE_FILTER_POINT);
    tilemap_init_defs(&g->tilesetTex, g->tileDefs);

    // Initialize character sprite atlas
    sprite_atlas_init(&g->spriteAtlas);

    // Initialize test sprite animation
    anim_state_init(&g->testAnimP1, ANIM_WALK, DIR_UP, 8.0f);
    anim_state_init(&g->testAnimP2, ANIM_WALK, DIR_UP, 8.0f);

    // Initialize split-screen viewports and players
    viewport_init_split_screen(g);

    return true;
}

void game_update(GameState *g) {
    float deltaTime = GetFrameTime();

    // Update test sprite animation
    anim_state_update(&g->testAnimP1, deltaTime);
    anim_state_update(&g->testAnimP2, deltaTime);

    // Update both players
    player_update(&g->players[0], deltaTime);
    player_update(&g->players[1], deltaTime);
}

void game_render(GameState *g) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Render Player 1's viewport
    viewport_begin(&g->players[0]);
    viewport_draw_tilemap(&g->players[0], g->tileDefs);
    DrawText("PLAYER 1",
             g->players[0].playArea.x + 40,
             g->players[0].playArea.y + 40,
             40, DARKGREEN);

    // Draw test card in player 1's first card slot
    float cardScale = DEFAULT_CARD_SCALE;
    float cw = CARD_WIDTH * cardScale;
    float ch = CARD_HEIGHT * cardScale;
    CardSlot *slot = player_get_slot(&g->players[0], 0);
    if (slot) {
        Vector2 cardPos = {
            slot->worldPos.x - cw / 2.0f,
            slot->worldPos.y - ch / 2.0f
        };
        card_draw(&g->cardAtlas, &g->testVisual, cardPos, cardScale);

    }

    // Test sprite: player 1
    sprite_draw(&g->spriteAtlas.base, &g->testAnimP1,
                player_lane_pos(&g->players[0], 1, 0.8f), 2.0f);

    // Debug: draw card slot positions
    viewport_draw_card_slots_debug(&g->players[0]);
    viewport_end();

    // Render Player 2's viewport
    viewport_begin(&g->players[1]);
    viewport_draw_tilemap(&g->players[1], g->tileDefs);
    DrawText("PLAYER 2",
             g->players[1].playArea.x + 40,
             g->players[1].playArea.y + 40,
             40, MAROON);

    // Test sprite: player 2
    sprite_draw(&g->spriteAtlas.base, &g->testAnimP2,
                player_lane_pos(&g->players[1], 1, 0.8f), 2.0f);

    viewport_draw_card_slots_debug(&g->players[1]);
    viewport_end();

    EndDrawing();
}

void game_cleanup(GameState *g) {
    // Cleanup players (frees tilemaps)
    player_cleanup(&g->players[0]);
    player_cleanup(&g->players[1]);

    sprite_atlas_free(&g->spriteAtlas);
    card_atlas_free(&g->cardAtlas);
    UnloadTexture(g->tilesetTex);
    CloseWindow();
    cards_free(&g->deck);
    db_close(&g->db);
}

int main(void) {
    GameState game = {0};

    if (!game_init(&game)) {
        return 1;
    }

    while (!WindowShouldClose()) {
        game_update(&game);
        game_render(&game);
    }

    game_cleanup(&game);
    return 0;
}
