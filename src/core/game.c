//
// Created by Nathan Davis on 2/16/26.
//

#include "game.h"
#include "config.h"
#include "../logic/card_effects.h"
#include "../rendering/viewport.h"
#include "../rendering/sprite_renderer.h"
#include "../systems/player.h"
#include "../entities/entities.h"
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

// Simulate a knight card play through the full production code path:
// cards_find → currentPlayerIndex → card_action_play → play_knight
//   → spawn_troop_from_card → troop_create_data_from_card → troop_spawn
static void game_test_play_knight(GameState *g, int playerIndex) {
    Card *card = cards_find(&g->deck, "KNIGHT_001");
    if (!card) {
        printf("[TEST] KNIGHT_001 not found in deck\n");
        return;
    }
    g->currentPlayerIndex = playerIndex;
    card_action_play(card, g);
}

static void game_handle_test_input(GameState *g) {
    // Player 1: key 1
    if (IsKeyPressed(KEY_ONE)) game_test_play_knight(g, 0);

    // Player 2: key Q
    if (IsKeyPressed(KEY_Q)) game_test_play_knight(g, 1);
}

void game_update(GameState *g) {
    float deltaTime = GetFrameTime();

    game_handle_test_input(g);

    // Update both players
    player_update(&g->players[0], deltaTime);
    player_update(&g->players[1], deltaTime);

    // Update entities for both players
    player_update_entities(&g->players[0], g, deltaTime);
    player_update_entities(&g->players[1], g, deltaTime);
}

// Draw entities for a viewport. Owner's entities draw normally; opponent's
// crossed entities appear at a mirrored position walking down.
static void game_draw_entities_for_viewport(GameState *g, const Player *viewportPlayer) {
    for (int pid = 0; pid < 2; pid++) {
        const Player *owner = &g->players[pid];
        const Player *opponent = &g->players[1 - pid];

        for (int i = 0; i < owner->entityCount; i++) {
            const Entity *e = owner->entities[i];

            if (viewportPlayer == owner) {
                // Draw in owner's viewport — scissor clips at the edge naturally
                entity_draw(e);
            } else if (e->position.y < owner->playArea.y) {
                // Entity has crossed the border — draw in opponent's viewport
                float lateral = (e->position.x - owner->playArea.x) / owner->playArea.width;
                float mirroredLateral = 1.0f - lateral;
                float depth = owner->playArea.y - e->position.y;
                Vector2 mappedPos = {
                    opponent->playArea.x + mirroredLateral * opponent->playArea.width,
                    opponent->playArea.y + depth
                };

                AnimState crossed = e->anim;
                crossed.dir = DIR_DOWN;
                sprite_draw(e->sprite, &crossed, mappedPos, e->spriteScale);
            }
        }
    }
}

void game_render(GameState *g) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Render Player 1's viewport
    viewport_begin(&g->players[0]);
    viewport_draw_tilemap(&g->players[0]);
    game_draw_entities_for_viewport(g, &g->players[0]);
    DrawText("PLAYER 1",
             g->players[0].playArea.x + 40,
             g->players[0].playArea.y + 40,
             40, DARKGREEN);
    // viewport_draw_card_slots_debug(&g->players[0]);
    viewport_end();

    // Render Player 2's viewport
    viewport_begin(&g->players[1]);
    viewport_draw_tilemap(&g->players[1]);
    game_draw_entities_for_viewport(g, &g->players[1]);
    DrawText("PLAYER 2",
             g->players[1].playArea.x + 40,
             g->players[1].playArea.y + 40,
             40, MAROON);
    // viewport_draw_card_slots_debug(&g->players[1]);
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
