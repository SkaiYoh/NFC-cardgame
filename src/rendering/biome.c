//
// Created by Nathan Davis on 2/16/26.
//

#include "biome.h"
#include "../core/config.h"
#include <stdio.h>
#include <string.h>

#define R(x, y, w, h)  (Rectangle){ (x), (y), (w), (h) }

// ---- Per-biome definitions ----

static void biome_define_grass(BiomeDef *b) {
    b->texturePath = GRASS_TILESET_PATH;
    b->blockCount = 2;

    // Block 0: plain grass (4x4 grid at origin 0,0)
    b->blocks[0] = (TileBlock){ .srcX = 0, .srcY = 0, .cols = 4, .rows = 4, .tileW = 32, .tileH = 32 };
    // Block 1: flower variants (4x4 grid at origin 128,0)
    b->blocks[1] = (TileBlock){ .srcX = 128, .srcY = 0, .cols = 4, .rows = 4, .tileW = 32, .tileH = 32 };

    b->blockWeights[0] = 80;
    b->blockWeights[1] = 20;

    b->tileScale = 1.0f;  // 32px native, no scaling needed

    // No detail overlay for grass (flowers are already in the base blocks)
    b->detailTexturePath = NULL;
    b->detailDensity = 0;
}

static void biome_define_undead(BiomeDef *b) {
    // Ground_rocks.png: 416x1392, 16px tiles, 26 columns.
    // This is an autotile sheet — almost all tiles are rock cliff edges.
    // Only one seamless ground fill tile exists: index 54 at pixel (32, 32),
    // solid rgb(111,115,106). The Tiled map uses this single tile for all
    // ground fill and layers details.png on top for visual variety.
    b->texturePath = UNDEAD_TILESET_PATH;
    b->blockCount = 1;

    // Block 0: solid ground fill (row 2, col 2 — the only seamless tile)
    b->blocks[0] = (TileBlock){ .srcX = 32, .srcY = 32, .cols = 1, .rows = 1, .tileW = 16, .tileH = 16 };

    b->blockWeights[0] = 100;

    b->tileScale = 2.0f;  // 16px native, scale 2x to match 32px grass tiles

    // Detail overlay from details.png (640x224, 16px tiles, 40 cols x 14 rows).
    // The sheet has 3 color columns (cols 0-12, 13-25, 26-38) with same patterns.
    // Using the first column (dark palette, matching the ground color).
    // Manual sprite definitions — each R(x, y, w, h) is an exact pixel rect.
    b->detailTexturePath = UNDEAD_DETAIL_PATH;
    b->detailBlockCount = 0;  // manual mode: detailDefs[] populated directly

    int d = 0;
    // Dead thorns / plant scatter (row 1, cols 1-4)
    // dark brown plant scatter
    // b->detailDefs[d++] = (TileDef){ .source = R(18, 17, 27, 28) };
    b->detailDefs[d++] = (TileDef){ .source = R(52, 20, 22, 23) };
    b->detailDefs[d++] = (TileDef){ .source = R(83, 34, 11, 11) };
    b->detailDefs[d++] = (TileDef){ .source = R(100, 36, 8, 8) };
    b->detailDefs[d++] = (TileDef){ .source = R(117, 37, 5, 6) };
    // light brown plant scatter
    b->detailDefs[d++] = (TileDef){ .source = R(146, 32, 12, 16) };
    b->detailDefs[d++] = (TileDef){ .source = R(163, 34, 10, 13) };
    b->detailDefs[d++] = (TileDef){ .source = R(180, 36, 8, 10) };

    // Pebbles
    b->detailDefs[d++] = (TileDef){ .source = R(26, 140, 12, 9) };
    b->detailDefs[d++] = (TileDef){ .source = R(100, 149, 9, 5) };
    b->detailDefs[d++] = (TileDef){ .source = R(117, 149, 5, 4) };
    b->detailDefs[d++] = (TileDef){ .source = R(139, 134, 16, 14) };
    b->detailDefs[d++] = (TileDef){ .source = R(165, 134, 17, 13) };

    // Crack patterns (row 8, cols 2-3)
    b->detailDefs[d++] = (TileDef){ .source = R(18, 67, 40, 21) };
    b->detailDefs[d++] = (TileDef){ .source = R(20, 96, 28, 22) };
    b->detailDefs[d++] = (TileDef){ .source = R(85, 103, 22, 17) };
    b->detailDefs[d++] = (TileDef){ .source = R(188, 107, 10, 10) };

    b->detailDefCount = d;

    b->detailDensity = 18;  // 25% of tiles get a detail overlay
}

static void biome_define_snow(BiomeDef *b) {
    // Placeholder: reuses grass tileset until snow assets exist
    b->texturePath = GRASS_TILESET_PATH;
    b->blockCount = 2;

    b->blocks[0] = (TileBlock){ .srcX = 0, .srcY = 0, .cols = 4, .rows = 4, .tileW = 32, .tileH = 32 };
    b->blocks[1] = (TileBlock){ .srcX = 128, .srcY = 0, .cols = 4, .rows = 4, .tileW = 32, .tileH = 32 };

    b->blockWeights[0] = 90;
    b->blockWeights[1] = 10;

    b->tileScale = 1.0f;

    b->detailTexturePath = NULL;
    b->detailDensity = 0;
}

static void biome_define_swamp(BiomeDef *b) {
    // Placeholder: reuses grass tileset with more flowers until swamp assets exist
    b->texturePath = GRASS_TILESET_PATH;
    b->blockCount = 2;

    b->blocks[0] = (TileBlock){ .srcX = 0, .srcY = 0, .cols = 4, .rows = 4, .tileW = 32, .tileH = 32 };
    b->blocks[1] = (TileBlock){ .srcX = 128, .srcY = 0, .cols = 4, .rows = 4, .tileW = 32, .tileH = 32 };

    b->blockWeights[0] = 60;
    b->blockWeights[1] = 40;

    b->tileScale = 1.0f;

    b->detailTexturePath = NULL;
    b->detailDensity = 0;
}

// ---- Internal: build flat TileDef array from blocks ----

static void biome_build_tiledefs(BiomeDef *b) {
    int idx = 0;
    for (int i = 0; i < b->blockCount && idx < TILE_COUNT; i++) {
        TileBlock *blk = &b->blocks[i];
        b->blockStart[i] = idx;
        int count = 0;
        for (int r = 0; r < blk->rows && idx < TILE_COUNT; r++) {
            for (int c = 0; c < blk->cols && idx < TILE_COUNT; c++) {
                b->tileDefs[idx] = (TileDef){
                    .texture = &b->texture,
                    .source  = (Rectangle){
                        blk->srcX + c * blk->tileW,
                        blk->srcY + r * blk->tileH,
                        blk->tileW,
                        blk->tileH
                    }
                };
                idx++;
                count++;
            }
        }
        b->blockSize[i] = count;
    }
    b->tileDefCount = idx;
}

static void biome_build_detail_defs(BiomeDef *b) {
    int idx = 0;
    for (int i = 0; i < b->detailBlockCount && idx < MAX_DETAIL_DEFS; i++) {
        TileBlock *blk = &b->detailBlocks[i];
        b->detailBlockStart[i] = idx;
        int count = 0;
        for (int r = 0; r < blk->rows && idx < MAX_DETAIL_DEFS; r++) {
            for (int c = 0; c < blk->cols && idx < MAX_DETAIL_DEFS; c++) {
                b->detailDefs[idx] = (TileDef){
                    .texture = &b->detailTexture,
                    .source  = (Rectangle){
                        blk->srcX + c * blk->tileW,
                        blk->srcY + r * blk->tileH,
                        blk->tileW,
                        blk->tileH
                    }
                };
                idx++;
                count++;
            }
        }
        b->detailBlockSize[i] = count;
    }
    b->detailDefCount = idx;
}

// ---- Public API ----

void biome_init_all(BiomeDef biomeDefs[BIOME_COUNT]) {
    memset(biomeDefs, 0, sizeof(BiomeDef) * BIOME_COUNT);

    biome_define_grass(&biomeDefs[BIOME_GRASS]);
    biome_define_undead(&biomeDefs[BIOME_UNDEAD]);
    biome_define_snow(&biomeDefs[BIOME_SNOW]);
    biome_define_swamp(&biomeDefs[BIOME_SWAMP]);

    for (int i = 0; i < BIOME_COUNT; i++) {
        BiomeDef *b = &biomeDefs[i];
        if (!b->texturePath) continue;

        b->texture = LoadTexture(b->texturePath);
        SetTextureFilter(b->texture, TEXTURE_FILTER_POINT);
        b->loaded = true;

        biome_build_tiledefs(b);

        // Load detail overlay texture if defined
        if (b->detailTexturePath && (b->detailBlockCount > 0 || b->detailDefCount > 0)) {
            b->detailTexture = LoadTexture(b->detailTexturePath);
            SetTextureFilter(b->detailTexture, TEXTURE_FILTER_POINT);
            b->detailLoaded = true;

            if (b->detailBlockCount > 0) {
                // Grid-based: build detail defs from blocks
                biome_build_detail_defs(b);
            } else {
                // Manual mode: wire up texture pointers for pre-filled detailDefs
                for (int j = 0; j < b->detailDefCount; j++) {
                    b->detailDefs[j].texture = &b->detailTexture;
                }
            }

            printf("Biome %d: loaded %d detail defs from '%s' (density %d%%)\n",
                   i, b->detailDefCount, b->detailTexturePath, b->detailDensity);
        }

        printf("Biome %d: loaded %d tile defs from '%s'\n",
               i, b->tileDefCount, b->texturePath);
    }
}

void biome_free_all(BiomeDef biomeDefs[BIOME_COUNT]) {
    for (int i = 0; i < BIOME_COUNT; i++) {
        if (biomeDefs[i].loaded) {
            UnloadTexture(biomeDefs[i].texture);
            biomeDefs[i].loaded = false;
        }
        if (biomeDefs[i].detailLoaded) {
            UnloadTexture(biomeDefs[i].detailTexture);
            biomeDefs[i].detailLoaded = false;
        }
    }
}

void biome_copy_tiledefs(const BiomeDef *biome, TileDef outDefs[TILE_COUNT]) {
    memcpy(outDefs, biome->tileDefs, sizeof(TileDef) * TILE_COUNT);
}

void biome_copy_detail_defs(const BiomeDef *biome, TileDef outDefs[MAX_DETAIL_DEFS]) {
    memcpy(outDefs, biome->detailDefs, sizeof(TileDef) * MAX_DETAIL_DEFS);
}

int biome_tile_count(const BiomeDef *biome) {
    return biome->tileDefCount;
}
