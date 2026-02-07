#include "db.h"
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

    // test db query
    PGresult *res = db_query(&db, "SELECT card_id, name FROM cards");

    if (res) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; i++) {
            printf("%s - %s\n",
                   PQgetvalue(res, i, 0),
                   PQgetvalue(res, i, 1));
        }
        PQclear(res);
    }

    // raylib test
    const int screenWidth = 1920;
    const int screenHeight = 1080;

    InitWindow(screenWidth, screenHeight, "Raylib Test");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(BLACK);

        DrawText("I fucking hate you sammy", 100, 100, 40, RAYWHITE);

        EndDrawing();
    }

    CloseWindow();

    db_close(&db);
    return 0;
}
