//
// Farmer unit behavior -- seek/mine/return/deposit ore loop.
//

#ifndef NFC_CARDGAME_FARMER_H
#define NFC_CARDGAME_FARMER_H

#include "../core/types.h"

// Run one frame of the farmer state machine.
// Called from entity_update() instead of the combat path.
void farmer_update(Entity *e, GameState *gs, float deltaTime);

// Handle farmer death: release ore claim, award carried ore to opponent.
// Idempotent -- safe to call from multiple kill paths.
void farmer_on_death(Entity *farmer, GameState *gs);

#endif //NFC_CARDGAME_FARMER_H
