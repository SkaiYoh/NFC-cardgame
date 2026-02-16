//
// Created by Nathan Davis on 2/16/26.
//

#include "player.h"
#include <string.h>
#include <stdio.h>

static Vector2 rect_center(Rectangle r) {
    return (Vector2){
        r.x + r.width / 2.0f,
        r.y + r.height / 2.0f
    };
}

void player_init(Player *p, int id, Rectangle playArea, Rectangle screenArea,
                 float cameraRotation, BiomeType startBiome,
                 TileDef *tileDefs, float tileSize, unsigned int seed) {
    (void)tileDefs; // void for now, until we get multi biome setup
    memset(p, 0, sizeof(Player));

    p->id = id;
    p->playArea = playArea;
    p->screenArea = screenArea;
    p->cameraRotation = cameraRotation;
    p->biome = startBiome;

    // Setup camera
    p->camera = (Camera2D){0};
    p->camera.target = rect_center(playArea);
    p->camera.offset = (Vector2){
        screenArea.x + screenArea.width / 2.0f,
        screenArea.y + screenArea.height / 2.0f
    };
    p->camera.rotation = cameraRotation;
    p->camera.zoom = 1.0f;

    // Create tilemap
    p->tilemap = tilemap_create(playArea, tileSize, seed);

    // Initialize card slots
    player_init_card_slots(p);

    // Initialize energy (defaults, will be configurable in Phase 5)
    p->energy = 5.0f;
    p->maxEnergy = 10.0f;
    p->energyRegenRate = 1.0f;

    // No entities yet
    p->entityCount = 0;
    p->base = NULL;

    printf("Player %d initialized\n", id);
}

void player_init_card_slots(Player *p) {
    // Divide play area into 3 lanes (left, center, right)
    float laneWidth = p->playArea.width / 3.0f;

    // Spawn position: near the player's edge of the play area
    // For player 0: near top of their area
    // For player 1: near bottom of their area (but coords are same due to rotation)
    float spawnY = p->playArea.y + p->playArea.height * 0.8f;

    for (int i = 0; i < NUM_CARD_SLOTS; i++) {
        p->slots[i].worldPos = (Vector2){
            p->playArea.x + (i + 0.5f) * laneWidth,
            spawnY
        };
        p->slots[i].activeCard = NULL;
        p->slots[i].isOccupied = false;
        p->slots[i].cooldownTimer = 0.0f;
    }
}

void player_update(Player *p, float deltaTime) {
    // Update energy regeneration
    if (p->energy < p->maxEnergy) {
        p->energy += p->energyRegenRate * deltaTime;
        if (p->energy > p->maxEnergy) {
            p->energy = p->maxEnergy;
        }
    }

    // Update card slot cooldowns
    for (int i = 0; i < NUM_CARD_SLOTS; i++) {
        if (p->slots[i].cooldownTimer > 0.0f) {
            p->slots[i].cooldownTimer -= deltaTime;
            if (p->slots[i].cooldownTimer < 0.0f) {
                p->slots[i].cooldownTimer = 0.0f;
            }
        }
    }

    // Entity updates will be added in Phase 7
}

void player_cleanup(Player *p) {
    tilemap_free(&p->tilemap);

    // Entity cleanup will be added in Phase 7
    for (int i = 0; i < p->entityCount; i++) {
        // entity_destroy(p->entities[i]);
        p->entities[i] = NULL;
    }
    p->entityCount = 0;

    printf("Player %d cleaned up\n", p->id);
}

void player_add_entity(Player *p, Entity *entity) {
    if (p->entityCount >= MAX_ENTITIES) {
        fprintf(stderr, "Player %d: entity limit reached\n", p->id);
        return;
    }
    p->entities[p->entityCount++] = entity;
}

void player_remove_entity(Player *p, int entityID) {
    for (int i = 0; i < p->entityCount; i++) {
        // Entity ID comparison will be implemented in Phase 7
        // For now, stub
        (void)entityID;
    }
}

Entity *player_find_entity(Player *p, int entityID) {
    for (int i = 0; i < p->entityCount; i++) {
        // Entity ID comparison will be implemented in Phase 7
        (void)entityID;
    }
    return NULL;
}

CardSlot *player_get_slot(Player *p, int slotIndex) {
    if (slotIndex < 0 || slotIndex >= NUM_CARD_SLOTS) {
        return NULL;
    }
    return &p->slots[slotIndex];
}

bool player_slot_is_available(Player *p, int slotIndex) {
    if (slotIndex < 0 || slotIndex >= NUM_CARD_SLOTS) {
        return false;
    }
    return !p->slots[slotIndex].isOccupied &&
           p->slots[slotIndex].cooldownTimer <= 0.0f;
}
