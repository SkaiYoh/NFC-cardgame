//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_TYPES_H

#include "../../lib/raylib.h"
#include "config.h"
#include "../data/db.h"
#include "../data/cards.h"
#include "../rendering/card_renderer.h"
#include "../rendering/tilemap_renderer.h"
#include "../rendering/sprite_renderer.h"

// Forward declarations
typedef struct Entity Entity;
typedef struct Player Player;
typedef struct GameState GameState;

// Constants
#define NUM_CARD_SLOTS 3
#define MAX_ENTITIES 64

// Biome types
typedef enum {
    BIOME_GRASS,
    BIOME_DESERT,
    BIOME_SNOW,
    BIOME_SWAMP,
    BIOME_COUNT
} BiomeType;

// Card slot - represents a physical NFC reader position
typedef struct {
    Vector2 worldPos;       // Spawn position in world coordinates
    Card *activeCard;       // Currently placed card (NULL if empty)
    bool isOccupied;        // Whether a card is on this slot
    float cooldownTimer;    // Cooldown before slot can be used again
} CardSlot;

// Player state
struct Player {
    int id;                         // 0 or 1
    Rectangle playArea;             // World space play area
    Rectangle screenArea;           // Screen space viewport
    Camera2D camera;                // Camera for this player's view
    float cameraRotation;           // 90 or -90 for split screen orientation

    // Tilemap
    TileMap tilemap;
    BiomeType biome;

    // Card slots (3 NFC readers per player)
    CardSlot slots[NUM_CARD_SLOTS];

    // Entities (troops, buildings) - will be expanded in Phase 7
    Entity *entities[MAX_ENTITIES];
    int entityCount;

    // Energy system - will be expanded in Phase 5
    float energy;
    float maxEnergy;
    float energyRegenRate;

    // Base building reference (for win condition)
    Entity *base;
};

// Game state
struct GameState {
    // Database & cards
    DB db;
    Deck deck;
    CardAtlas cardAtlas;

    // Players
    Player players[2];

    // Shared tileset (both players use same tile definitions)
    Texture2D tilesetTex;
    TileDef tileDefs[TILE_COUNT];

    // Character sprites (shared by all entities)
    SpriteAtlas spriteAtlas;

    // Screen layout
    int halfWidth;  // Half screen width for split screen

    // Test visual (temporary, for development)
    CardVisual testVisual;
    AnimState testAnimP1;
    AnimState testAnimP2;
};

#endif //NFC_CARDGAME_TYPES_H
