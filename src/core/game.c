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

    // test play every card in the deck
    for (int i = 0; i < g->deck.count; i++) {
        card_action_play(&g->deck.cards[i], NULL);
    }

    // Initialize biome definitions (loads textures, builds tile defs)
    biome_init_all(g->biomeDefs);

    // Initialize character sprite atlas
    sprite_atlas_init(&g->spriteAtlas);

    // Initialize split-screen viewports and players
    viewport_init_split_screen(g);

    return true;
}

void game_update(GameState *g) {
    float deltaTime = GetFrameTime();

    // Update both players
    player_update(&g->players[0], deltaTime);
    player_update(&g->players[1], deltaTime);
}

void game_render(GameState *g) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Render Player 1's viewport
    viewport_begin(&g->players[0]);
    viewport_draw_tilemap(&g->players[0]);
    DrawText("PLAYER 1",
             g->players[0].playArea.x + 40,
             g->players[0].playArea.y + 40,
             40, DARKGREEN);

    // Debug: draw card slot positions
    viewport_draw_card_slots_debug(&g->players[0]);
    viewport_end();

    // --------------- END PLAYER 1, START PLAYER 2 --------------- //

    // Render Player 2's viewport
    viewport_begin(&g->players[1]);
    viewport_draw_tilemap(&g->players[1]);
    DrawText("PLAYER 2",
             g->players[1].playArea.x + 40,
             g->players[1].playArea.y + 40,
             40, MAROON);

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
    biome_free_all(g->biomeDefs);
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
