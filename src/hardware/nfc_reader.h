//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_NFC_READER_H
#define NFC_CARDGAME_NFC_READER_H

#include <stdbool.h>

typedef struct {
    char uid[32];       // Card UID string from NFC reader
    int  readerIndex;   // Which reader slot fired (0–2)
    int  playerIndex;   // Which player this reader belongs to (0 or 1)
} NFCEvent;

#endif //NFC_CARDGAME_NFC_READER_H