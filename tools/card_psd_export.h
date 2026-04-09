#ifndef NFC_CARDGAME_CARD_PSD_EXPORT_H
#define NFC_CARDGAME_CARD_PSD_EXPORT_H

#include "../src/rendering/card_renderer.h"
#include <stdbool.h>

bool card_export_psd(const CardAtlas *atlas, const CardVisual *visual, const char *path);

#endif //NFC_CARDGAME_CARD_PSD_EXPORT_H
