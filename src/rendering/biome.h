//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_BIOME_H
#define NFC_CARDGAME_BIOME_H

#include "tilemap_renderer.h"
#include "../../lib/raylib.h"
#include <stdbool.h>
#include <string.h>

// Biome types
typedef enum {
    BIOME_GRASS,
    BIOME_UNDEAD,
    BIOME_SNOW,
    BIOME_SWAMP,
    BIOME_COUNT
} BiomeType;

// Describes a rectangular grid of tiles within a sprite sheet.
// e.g. srcX=128, srcY=0, cols=4, rows=4, tileW=32, tileH=32
// reads a 4x4 block of 32px tiles starting at pixel (128, 0).
typedef struct {
    int srcX;
    int srcY;
    int cols;
    int rows;
    int tileW;
    int tileH;
} TileBlock;

#define MAX_TILE_BLOCKS 4

#define MAX_DETAIL_DEFS 32

// Full definition of a biome's tileset mapping and generation weights.
typedef struct BiomeDef {
    const char *texturePath;
    Texture2D   texture;
    bool        loaded;

    TileBlock   blocks[MAX_TILE_BLOCKS];
    int         blockCount;

    // Compiled flat TileDef array built from blocks
    TileDef     tileDefs[TILE_COUNT];
    int         tileDefCount;

    // Per-block index ranges into tileDefs[]
    int         blockStart[MAX_TILE_BLOCKS];
    int         blockSize[MAX_TILE_BLOCKS];

    // Distribution weights for tilemap generation (per block)
    int         blockWeights[MAX_TILE_BLOCKS];

    // Scale factor applied to source tiles so different native sizes render equally.
    // e.g. 1.0 for 32px tiles, 2.0 for 16px tiles (scales up to match 32px).
    float       tileScale;

    // Detail overlay (transparent tiles drawn on top of base ground)
    const char *detailTexturePath;  // NULL if no detail layer
    Texture2D   detailTexture;
    bool        detailLoaded;

    TileBlock   detailBlocks[MAX_TILE_BLOCKS];
    int         detailBlockCount;

    TileDef     detailDefs[MAX_DETAIL_DEFS];
    int         detailDefCount;

    int         detailBlockStart[MAX_TILE_BLOCKS];
    int         detailBlockSize[MAX_TILE_BLOCKS];
    int         detailBlockWeights[MAX_TILE_BLOCKS];

    int         detailDensity;  // percentage chance (0-100) a cell gets a detail
} BiomeDef;

// Initialize all biome definitions, load textures, build TileDef arrays.
void biome_init_all(BiomeDef biomeDefs[BIOME_COUNT]);

// Unload all biome textures.
void biome_free_all(BiomeDef biomeDefs[BIOME_COUNT]);

// Copy a biome's TileDef array into a player-local buffer.
void biome_copy_tiledefs(const BiomeDef *biome, TileDef outDefs[TILE_COUNT]);

// Copy a biome's detail TileDef array into a player-local buffer.
void biome_copy_detail_defs(const BiomeDef *biome, TileDef outDefs[MAX_DETAIL_DEFS]);

// Get the number of valid tile definitions for a biome.
int biome_tile_count(const BiomeDef *biome);

#endif //NFC_CARDGAME_BIOME_H
