//
// Assault slot reservation for static combat targets.
//
// Bases/buildings expose a front-facing primary arc plus a secondary queue
// ring. Combat troops reserve these attacker-center positions so assaults
// spread across a readable front instead of collapsing on the target pivot.
//

#ifndef NFC_CARDGAME_ASSAULT_SLOTS_H
#define NFC_CARDGAME_ASSAULT_SLOTS_H

#include "../core/types.h"

void assault_slots_build_for_base(Entity *base);

AssaultSlotKind assault_slots_reserve_for(Entity *base, int entityId,
                                          Vector2 fromPos, int *outSlotIndex);

bool assault_slots_try_promote(Entity *base, int entityId, int queueSlotIndex,
                               int *outPrimarySlotIndex);

void assault_slots_release(Entity *base, AssaultSlotKind kind, int slotIndex,
                           int entityId);

void assault_slots_release_for_entity(Entity *base, int entityId);

Vector2 assault_slots_get_position(const Entity *base, AssaultSlotKind kind,
                                   int slotIndex);

int assault_slots_primary_count(const Entity *base);
int assault_slots_queue_count(const Entity *base);
const AssaultSlot *assault_slots_primary_at(const Entity *base, int slotIndex);
const AssaultSlot *assault_slots_queue_at(const Entity *base, int slotIndex);

#endif //NFC_CARDGAME_ASSAULT_SLOTS_H
