//
// Hand UI -- per-seat placeholder card strip along each player's outer edge.
//
// The hand bar is visual-only at this stage: it paints an opaque background
// on the player's outer-edge strip and draws HAND_CARD_COUNT identical
// placeholder cards rotated so they read upright to each player. Future
// work will replace the static placeholder with live deck/slot state.
//

#ifndef NFC_CARDGAME_HAND_UI_H
#define NFC_CARDGAME_HAND_UI_H

#include "../core/types.h"

// Load the shared hand placeholder texture once at startup.
// Safe to call before the window exists only if raylib is initialized.
Texture2D hand_ui_load_placeholder(void);

// Unload the shared placeholder texture. No-op if the texture id is zero.
void hand_ui_unload_placeholder(Texture2D texture);

// Draw the hand bar (background fill + HAND_CARD_COUNT rotated placeholder
// cards) inside the player's handArea. Must be called outside any scissor
// or camera scope (i.e. at screen-space draw time).
void hand_ui_draw(const Player *p, Texture2D placeholder);

// Pure-math helper: compute the world/screen-space center of the cardIndex-th
// hand slot inside `handArea`. The stacking axis is the handArea's long axis
// (height on a 180x1080 strip). Exposed for unit testing.
Vector2 hand_ui_card_center_for_index(Rectangle handArea, int cardIndex);

#endif //NFC_CARDGAME_HAND_UI_H
