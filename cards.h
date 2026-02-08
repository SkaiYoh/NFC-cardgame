#ifndef CARDS_H
#define CARDS_H

#include "db.h"
#include <stdbool.h>

typedef struct {
    char *card_id;
    char *name;
    int   cost;
    char *type;
    char *rules_text;  /* may be NULL */
    char *data;        /* raw JSON string from jsonb column */
} Card;

typedef struct {
    Card *cards;
    int   count;
} Deck;

/* Load all cards from the database into the store.
 * Returns true on success, false on failure. */
bool cards_load(Deck *deck, DB *db);

/* Find a card by card_id. Returns pointer into store or NULL. */
Card *cards_find(Deck *deck, const char *card_id);

/* Free all memory owned by the store. */
void cards_free(Deck *deck);

#endif /* CARDS_H */
