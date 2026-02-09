#ifndef TILEMAP_H
#define TILEMAP_H

#include "raylib.h"

// Tile type indices - Grass (0-15)
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

// Tile type indices - Flowers (16-31)
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

#define TILE_COUNT 32

// Tile definition: which texture + source rect within that texture
typedef struct {
    Texture2D *texture;
    Rectangle source;
} TileDef;

// Tile map for a player area
typedef struct {
    int rows;
    int cols;
    int *cells;       // rows * cols tile indices
    float tileSize;   // rendered size per tile
    float originX;    // top-left x of the area this map covers
    float originY;    // top-left y of the area this map covers
} TileMap;

// Load the tileset texture and initialize all tile definitions.
// tex must already be loaded before calling this.
void tilemap_init_defs(Texture2D *tex, TileDef tileDefs[TILE_COUNT]);

// Create a tile map for a given area, randomized with grass + flowers
TileMap tilemap_create(Rectangle area, float tileSize, unsigned int seed);

// Draw a tile map using the tile definitions
void tilemap_draw(TileMap *map, TileDef tileDefs[TILE_COUNT]);

// Free a tile map
void tilemap_free(TileMap *map);

#endif
