#include "db.h"
#include "cards.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Tile definition: which texture + source rect within that texture
typedef struct {
    Texture2D *texture;
    Rectangle source;
} TileDef;

// Tile type indices
#define TILE_GRASS_0  0
#define TILE_GRASS_1  1
#define TILE_GRASS_2  2
#define TILE_GRASS_3  3
#define TILE_GRASS_4  4
#define TILE_GRASS_5  5
#define TILE_GRASS_6  6
#define TILE_GRASS_7  7
#define TILE_GRASS_8  8
#define TILE_GRASS_9  9
#define TILE_GRASS_10 10
#define TILE_GRASS_11 11
#define TILE_GRASS_12 12
#define TILE_GRASS_13 13
#define TILE_GRASS_14 14
#define TILE_GRASS_15 15
#define TILE_FLOWER_0  16
#define TILE_FLOWER_1  17
#define TILE_FLOWER_2  18
#define TILE_FLOWER_3  19
#define TILE_FLOWER_4  20
#define TILE_FLOWER_5  21
#define TILE_FLOWER_6  22
#define TILE_FLOWER_7  23
#define TILE_FLOWER_8  24
#define TILE_FLOWER_9  25
#define TILE_FLOWER_10 26
#define TILE_FLOWER_11 27
#define TILE_FLOWER_12 28
#define TILE_FLOWER_13 29
#define TILE_FLOWER_14 30
#define TILE_FLOWER_15 31
#define TILE_COUNT     32

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

    // Load grass tileset (128x128 grass area, 4x4 grid of 32x32 tiles)
    Texture2D grassTex = LoadTexture("assets/Pixel Art Top Down - Basic v1.2.3/Texture/TX Tileset Grass.png");
    SetTextureFilter(grassTex, TEXTURE_FILTER_POINT); // nearest-neighbor for crisp pixels

    float tileScale = 2.0f;
    float srcTileSize = 32.0f;
    float tileSize = srcTileSize * tileScale; // rendered size per tile

    // Tile definitions: map each tile index to a texture + source rect
    // Grass tiles are the top-left 128x128 of the tileset (4x4 grid of 32x32)
    TileDef tileDefs[TILE_COUNT];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            tileDefs[r * 4 + c] = (TileDef){
                .texture = &grassTex,
                .source  = (Rectangle){ c * 32, r * 32, 32, 32 }
            };
        }
    }
    // Flower tiles start at x=128, y=0 in the same spritesheet (4x4 grid of 32x32)
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            tileDefs[16 + r * 4 + c] = (TileDef){
                .texture = &grassTex,
                .source  = (Rectangle){ 128 + c * 32, r * 32, 32, 32 }
            };
        }
    }

    // Tile maps for each player area
    int mapCols = (int)(player1Area.width  / tileSize) + 1;
    int mapRows = (int)(player1Area.height / tileSize) + 1;

    int p1Map[mapRows][mapCols];
    int p2Map[mapRows][mapCols];

    // Fill both maps with randomized grass tiles
    srand(42); // fixed seed for consistent layout between runs
    for (int r = 0; r < mapRows; r++)
        for (int c = 0; c < mapCols; c++) {
            p1Map[r][c] = (rand() % 10 < 8) ? (rand() % 16) : (16 + rand() % 16);
            p2Map[r][c] = (rand() % 10 < 8) ? (rand() % 16) : (16 + rand() % 16);
        }

    // main Raylib window
    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        /* Player 1 */
        BeginScissorMode(0, 0, half, screenHeight); // Only draw inside this rectangle
        BeginMode2D(cam1); // Rotate camera 90 degrees
        // Tile background across player 1 area using tile map
        for (int row = 0; row < mapRows; row++) {
            for (int col = 0; col < mapCols; col++) {
                TileDef *td = &tileDefs[p1Map[row][col]];
                float tx = player1Area.x + col * tileSize;
                float ty = player1Area.y + row * tileSize;
                DrawTexturePro(*td->texture, td->source,
                    (Rectangle){ tx, ty, tileSize, tileSize },
                    (Vector2){ 0, 0 }, 0.0f, WHITE);
            }
        }
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
        // Tile background across player 2 area using tile map
        for (int row = 0; row < mapRows; row++) {
            for (int col = 0; col < mapCols; col++) {
                TileDef *td = &tileDefs[p2Map[row][col]];
                float tx = player2Area.x + col * tileSize;
                float ty = player2Area.y + row * tileSize;
                DrawTexturePro(*td->texture, td->source,
                    (Rectangle){ tx, ty, tileSize, tileSize },
                    (Vector2){ 0, 0 }, 0.0f, WHITE);
            }
        }
        DrawText("PLAYER 2", player2Area.x + 40, player2Area.y + 40, 40, MAROON);
        // DrawTexture(sprite, player2Area.x + 40, player2Area.y + 100, WHITE);
        EndMode2D();
        EndScissorMode();

        // Finish
        EndDrawing();

    }

    UnloadTexture(grassTex);
    CloseWindow();

    cards_free(&deck); // free cards in memory
    db_close(&db); // close db connection
    return 0;
}
