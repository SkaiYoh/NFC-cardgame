//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_PLAYER_H
#define NFC_CARDGAME_PLAYER_H

#include "../core/types.h"

// Initialize a player with their viewport area and starting biome
void player_init(Player *p, int id, Rectangle playArea, Rectangle screenArea,
                 float cameraRotation, BiomeType startBiome,
                 TileDef *tileDefs, float tileSize, unsigned int seed);

// Update player state (energy regen, entity updates, etc.)
void player_update(Player *p, float deltaTime);

// Clean up player resources
void player_cleanup(Player *p);

// Initialize the 3 card slot world positions based on play area
void player_init_card_slots(Player *p);

// Entity management (stubs for Phase 7)
void player_add_entity(Player *p, Entity *entity);
void player_remove_entity(Player *p, int entityID);
Entity *player_find_entity(Player *p, int entityID);

// Card slot access
CardSlot *player_get_slot(Player *p, int slotIndex);
bool player_slot_is_available(Player *p, int slotIndex);

#endif //NFC_CARDGAME_PLAYER_H
