#include "db.h"
#include "cards.h"
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

    // card sprite test
    Texture2D sprite = LoadTexture("assets/assassin_card.png");


    // main Raylib window
    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        /* Player 1 */
        BeginScissorMode(0, 0, half, screenHeight); // Only draw inside this rectangle
        BeginMode2D(cam1); // Rotate camera 90 degrees
        DrawRectangleRec(player1Area, Fade(GREEN, 0.2f)); // Background rectangle color
        DrawText("PLAYER 1", player1Area.x + 40, player1Area.y + 40, 40, DARKGREEN);
        DrawTexture(sprite, player1Area.x + 40, player1Area.y + 100, WHITE); // draws card
        EndMode2D(); // End camera rotation
        EndScissorMode(); // End scissor function

        /* Player 2 */
        BeginScissorMode(half, 0, half, screenHeight);
        BeginMode2D(cam2);
        DrawRectangleRec(player2Area, Fade(RED, 0.2f));
        DrawText("PLAYER 2", player2Area.x + 40, player2Area.y + 40, 40, MAROON);
        DrawTexture(sprite, player2Area.x + 40, player2Area.y + 100, WHITE);
        EndMode2D();
        EndScissorMode();

        // Finish
        EndDrawing();

    }

    CloseWindow();

    cards_free(&deck); // free cards in memory
    db_close(&db); // close db connection
    return 0;
}
