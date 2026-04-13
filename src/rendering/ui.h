//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_UI_H
#define NFC_CARDGAME_UI_H

#include "../core/types.h"

// Draws the sustenance counter anchored to a corner of `viewport` and rotated
// for the player's orientation. Uses the bitmap font sheet when available;
// falls back to the default font with a player-derived color if the texture
// has zero id. Color is never provided by the caller.
void ui_draw_sustenance_counter(const Player *p, Rectangle viewport,
                                float rotation, Texture2D letteringTexture);

// Draws a match-result overlay ("DRAW", "VICTORY", or "DEFEAT") centered on
// the player's battlefield area. Uses the bitmap font sheet when available;
// falls back to the default font with a text-derived color if the texture
// has zero id.
void ui_draw_match_result(const Player *p, const char *text, float rotation,
                          Texture2D letteringTexture);

#endif //NFC_CARDGAME_UI_H
