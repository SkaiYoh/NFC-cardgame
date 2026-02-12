#include "db.h"
#include "cards.h"
#include "card_render.h"
#include "card_action.h"
#include "tilemap.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080

typedef struct GameState {
    DB          db;
    Deck        deck;
    CardAtlas   cardAtlas;
    CardVisual  testVisual;
    Texture2D   grassTex;
    TileDef     tileDefs[TILE_COUNT];
    TileMap     p1Map;
    TileMap     p2Map;
    Rectangle   player1Area;
    Rectangle   player2Area;
    Camera2D    cam1;
    Camera2D    cam2;
    int         half;
} GameState;

static Vector2 rect_center(Rectangle r) {
    return (Vector2){
        r.x + r.width  / 2.0f,
        r.y + r.height / 2.0f
    };
}

static bool game_init(GameState *g) {
    // Database connection
    const char *conninfo = getenv("DB_CONNECTION");
    if (!db_init(&g->db, conninfo)) {
        return false;
    }

    // Load all cards into memory
    if (!cards_load(&g->deck, &g->db)) {
        db_close(&g->db);
        return false;
    }

    // Raylib window
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Raylib Test");
    SetTargetFPS(60);

    // Player areas
    g->half = SCREEN_WIDTH / 2;

    g->player1Area = (Rectangle){
        .x = 0,
        .y = 0,
        .width  = SCREEN_HEIGHT,
        .height = g->half
    };

    g->player2Area = (Rectangle){
        .x = g->half,
        .y = 0,
        .width  = SCREEN_HEIGHT,
        .height = g->half
    };

    // Player 1 camera
    g->cam1 = (Camera2D){0};
    g->cam1.target   = rect_center(g->player1Area);
    g->cam1.offset   = (Vector2){ g->half / 2.0f, SCREEN_HEIGHT / 2.0f };
    g->cam1.rotation = 90.0f;
    g->cam1.zoom     = 1.0f;

    // Player 2 camera
    g->cam2 = (Camera2D){0};
    g->cam2.target   = rect_center(g->player2Area);
    g->cam2.offset   = (Vector2){ g->half + g->half / 2.0f, SCREEN_HEIGHT / 2.0f };
    g->cam2.rotation = -90.0f;
    g->cam2.zoom     = 1.0f;

    // Card action handlers
    card_action_init();

    // Card atlas
    card_atlas_init(&g->cardAtlas);

    // Card visual from DB
    g->testVisual = card_visual_default();
    Card *testCard = cards_find(&g->deck, "FARMER_001");
    if (testCard && testCard->data) {
        printf("Card data from DB: %s\n", testCard->data);
        g->testVisual = card_visual_from_json(testCard->data);
    }

    // Demo: play each card in the deck to verify dispatch
    for (int i = 0; i < g->deck.count; i++) {
        card_action_play(&g->deck.cards[i], NULL);
    }

    // Tileset
    g->grassTex = LoadTexture("assets/Pixel Art Top Down - Basic v1.2.3/Texture/TX Tileset Grass.png");
    SetTextureFilter(g->grassTex, TEXTURE_FILTER_POINT);
    tilemap_init_defs(&g->grassTex, g->tileDefs);

    float tileScale = 2.0f;
    float tileSize  = 32.0f * tileScale;

    // Tile maps
    g->p1Map = tilemap_create(g->player1Area, tileSize, 42);
    g->p2Map = tilemap_create(g->player2Area, tileSize, 99);

    return true;
}

static void game_cleanup(GameState *g) {
    tilemap_free(&g->p1Map);
    tilemap_free(&g->p2Map);
    card_atlas_free(&g->cardAtlas);
    UnloadTexture(g->grassTex);
    CloseWindow();
    cards_free(&g->deck);
    db_close(&g->db);
}

int main() {
    GameState game = {0};

    if (!game_init(&game)) {
        return 1;
    }

    // Main loop
    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        /* Player 1 */
        BeginScissorMode(0, 0, game.half, SCREEN_HEIGHT);
        BeginMode2D(game.cam1);
        tilemap_draw(&game.p1Map, game.tileDefs);
        DrawText("PLAYER 1", game.player1Area.x + 40, game.player1Area.y + 40, 40, DARKGREEN);

        int rows = 1;
        int cols = 3;

        float cellWidth  = game.player1Area.width  / cols;
        float cellHeight = game.player1Area.height / rows;

        Vector2 cellCenters[1][3];

        for (int row = 0; row < rows; row++) {
            for (int col = 0; col < cols; col++) {
                Rectangle cell = {
                    game.player1Area.x + col * cellWidth,
                    game.player1Area.y + row * cellHeight,
                    cellWidth,
                    cellHeight
                };
                cellCenters[row][col] = rect_center(cell);
            }
        }

        // Draw card using modular renderer
        float cardScale = 2.5f;
        float cw = CARD_WIDTH  * cardScale;
        float ch = CARD_HEIGHT * cardScale;
        Vector2 cardPos = {
            cellCenters[0][0].x - cw / 2.0f,
            cellCenters[0][0].y - ch / 2.0f
        };
        card_draw(&game.cardAtlas, &game.testVisual, cardPos, cardScale);

        EndMode2D();
        EndScissorMode();

        /* Player 2 */
        BeginScissorMode(game.half, 0, game.half, SCREEN_HEIGHT);
        BeginMode2D(game.cam2);
        tilemap_draw(&game.p2Map, game.tileDefs);
        DrawText("PLAYER 2", game.player2Area.x + 40, game.player2Area.y + 40, 40, MAROON);
        EndMode2D();
        EndScissorMode();

        EndDrawing();
    }

    game_cleanup(&game);
    return 0;
}
