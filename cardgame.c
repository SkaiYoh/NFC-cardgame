#include "db.h"
#include "cards.h"
#include "tilemap.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    DB db; // create db struct

    // set connection to postgres db
    const char *conninfo = getenv("DB_CONNECTION");

    if (!db_init(&db, conninfo)) {
        return 1;
    }

    // load all cards into memory
    Deck deck;
    if (!cards_load(&deck, &db)) {
        db_close(&db);
        return 1;
    }

    // Raylib test
    const int screenWidth = 1920;
    const int screenHeight = 1080;

    InitWindow(screenWidth, screenHeight, "Raylib Test");
    SetTargetFPS(60);

    // Define Rectangle struct for each player area
    Rectangle player1Area;
    Rectangle player2Area;

    int half = screenWidth/ 2;

    // Player 1 (left)
    player1Area = (Rectangle){
        .x = 0,
        .y = 0,
        .width  = screenHeight,
        .height = half
    };

    // Player 2 (right)
    player2Area = (Rectangle){
        .x = half,
        .y = 0,
        .width  = screenHeight,
        .height = half
    };

    // function for finding the center of each player area
    Vector2 RectCenter(Rectangle r) {
        return (Vector2){
            r.x + r.width  / 2.0f,
            r.y + r.height / 2.0f
        };
    }

    // Player 1  camera rotation
    Camera2D cam1 = {0};
    cam1.target = RectCenter(player1Area);
    cam1.offset = (Vector2){ half / 2.0f, screenHeight / 2.0f };
    cam1.rotation = 90.0f;
    cam1.zoom = 1.0f;

    // Player 2 camera rotation
    Camera2D cam2 = {0};
    cam2.target = RectCenter(player2Area);
    cam2.offset = (Vector2){ half + half / 2.0f, screenHeight / 2.0f };
    cam2.rotation = -90.0f;
    cam2.zoom = 1.0f;

    // Load card textures
    Texture2D cardTex = LoadTexture("assets/assassin_card.png");
    printf("Card textures initialized\n");

    // Load tileset and initialize tile definitions
    Texture2D grassTex = LoadTexture("assets/Pixel Art Top Down - Basic v1.2.3/Texture/TX Tileset Grass.png");
    SetTextureFilter(grassTex, TEXTURE_FILTER_POINT); // nearest-neighbor for crisp pixels

    TileDef tileDefs[TILE_COUNT];
    tilemap_init_defs(&grassTex, tileDefs);

    float tileScale = 2.0f;
    float tileSize = 32.0f * tileScale;

    // Create tile maps for each player area
    TileMap p1Map = tilemap_create(player1Area, tileSize, 42);
    TileMap p2Map = tilemap_create(player2Area, tileSize, 99);

    // main Raylib window
    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        /* Player 1 */
        BeginScissorMode(0, 0, half, screenHeight); // Only draw inside this rectangle
        BeginMode2D(cam1); // Rotate camera 90 degrees
        tilemap_draw(&p1Map, tileDefs);
        DrawText("PLAYER 1", player1Area.x + 40, player1Area.y + 40, 40, DARKGREEN);

        int rows = 2;
        int cols = 3;

        float cellWidth  = player1Area.width  / cols;
        float cellHeight = player1Area.height / rows;

        Vector2 cellCenters[2][3];

        for (int row = 0; row < rows; row++) {
            for (int col = 0; col < cols; col++) {
                Rectangle cell = {
                    player1Area.x + col * cellWidth,
                    player1Area.y + row * cellHeight,
                    cellWidth,
                    cellHeight
                };
                cellCenters[row][col] = RectCenter(cell);
                // DEBUG: Display 2x3 grid and center dots
                // DrawRectangleLinesEx(cell, 2, YELLOW);
                // DrawCircleV(cellCenters[row][col], 5, RED);
            }
        }

        // card sprite test
        float cw = (float)cardTex.width;
        float ch = (float)cardTex.height;

        DrawTexturePro(cardTex,
            (Rectangle){ 0, 0, cw, ch },
            (Rectangle){ cellCenters[0][0].x, cellCenters[0][0].y, cw, ch },
            (Vector2){ cw / 2.0f, ch / 2.0f },
            0.0f, WHITE);

        EndMode2D(); // End camera rotation
        EndScissorMode(); // End scissor function

        /* Player 2 */
        BeginScissorMode(half, 0, half, screenHeight);
        BeginMode2D(cam2); // Rotate camera 90 degrees
        tilemap_draw(&p2Map, tileDefs);
        DrawText("PLAYER 2", player2Area.x + 40, player2Area.y + 40, 40, MAROON);
        // DrawTexture(sprite, player2Area.x + 40, player2Area.y + 100, WHITE);
        EndMode2D();
        EndScissorMode();

        // Finish
        EndDrawing();

    }

    tilemap_free(&p1Map);
    tilemap_free(&p2Map);
    UnloadTexture(grassTex);
    CloseWindow();

    cards_free(&deck); // free cards in memory
    db_close(&db); // close db connection
    return 0;
}
