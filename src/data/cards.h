//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_CARDS_H
#define NFC_CARDGAME_CARDS_H

#include "db.h"
#include <stdbool.h>

typedef struct {
    char *card_id;
    char *name;
    int   cost;
    char *type;
    char *rules_text;
    char *data;
} Card;

typedef struct {
    Card *cards;
    int   count;
} Deck;

bool cards_load(Deck *deck, DB *db);
Card *cards_find(Deck *deck, const char *card_id);
void cards_free(Deck *deck);

#endif //NFC_CARDGAME_CARDS_H
