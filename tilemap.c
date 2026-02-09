#include "tilemap.h"
#include <stdlib.h>

void tilemap_init_defs(Texture2D *tex, TileDef tileDefs[TILE_COUNT]) {
    // Grass tiles: top-left 128x128 of tileset (4x4 grid of 32x32)
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            tileDefs[r * 4 + c] = (TileDef){
                .texture = tex,
                .source  = (Rectangle){ c * 32, r * 32, 32, 32 }
            };
        }
    }

    // Flower tiles: start at x=128, y=0 (4x4 grid of 32x32)
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            tileDefs[16 + r * 4 + c] = (TileDef){
                .texture = tex,
                .source  = (Rectangle){ 128 + c * 32, r * 32, 32, 32 }
            };
        }
    }
}

TileMap tilemap_create(Rectangle area, float tileSize, unsigned int seed) {
    TileMap map;
    map.cols = (int)(area.width  / tileSize) + 1;
    map.rows = (int)(area.height / tileSize) + 1;
    map.tileSize = tileSize;
    map.originX = area.x;
    map.originY = area.y;
    map.cells = malloc(map.rows * map.cols * sizeof(int));

    srand(seed);
    for (int r = 0; r < map.rows; r++) {
        for (int c = 0; c < map.cols; c++) {
            map.cells[r * map.cols + c] =
                (rand() % 10 < 8) ? (rand() % 16) : (16 + rand() % 16);
        }
    }

    return map;
}

void tilemap_draw(TileMap *map, TileDef tileDefs[TILE_COUNT]) {
    for (int row = 0; row < map->rows; row++) {
        for (int col = 0; col < map->cols; col++) {
            TileDef *td = &tileDefs[map->cells[row * map->cols + col]];
            float tx = map->originX + col * map->tileSize;
            float ty = map->originY + row * map->tileSize;
            DrawTexturePro(*td->texture, td->source,
                (Rectangle){ tx, ty, map->tileSize, map->tileSize },
                (Vector2){ 0, 0 }, 0.0f, WHITE);
        }
    }
}

void tilemap_free(TileMap *map) {
    free(map->cells);
    map->cells = NULL;
}
