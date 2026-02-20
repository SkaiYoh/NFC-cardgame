//
// Created by Nathan Davis on 2/16/26.
//

#include "cards.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool cards_load(Deck *deck, DB *db) {
    if (!deck || !db) return false;

    memset(deck, 0, sizeof(Deck));

    PGresult *res = db_query(db,
        "SELECT card_id, name, cost, type, rules_text, data::text FROM cards");

    if (!res) {
        fprintf(stderr, "Failed to load cards from database\n");
        return false;
    }

    int rows = PQntuples(res);
    if (rows == 0) {
        PQclear(res);
        return true;
    }

    deck->cards = calloc(rows, sizeof(Card));
    if (!deck->cards) {
        fprintf(stderr, "Failed to allocate card storage\n");
        PQclear(res);
        return false;
    }

    deck->count = rows;

    for (int i = 0; i < rows; i++) {
        Card *c = &deck->cards[i];
        // TODO: strdup() can return NULL on OOM. If any strdup fails the Card has a NULL field and
        // TODO: the loop continues silently with a partially-initialized deck. Check each return value
        // TODO: and abort / partial-free on failure to avoid reading uninitialized data later.
        c->card_id    = strdup(PQgetvalue(res, i, 0));
        c->name       = strdup(PQgetvalue(res, i, 1));
        // TODO: atoi() returns 0 on any parse failure, indistinguishable from a legitimate 0-cost card.
        // TODO: Replace with strtol() + errno check to detect malformed cost values in the DB.
        c->cost       = atoi(PQgetvalue(res, i, 2));
        c->type       = strdup(PQgetvalue(res, i, 3));
        c->rules_text = PQgetisnull(res, i, 4) ? NULL : strdup(PQgetvalue(res, i, 4));
        // TODO: The data column NULL check is missing here — unlike rules_text, PQgetisnull is not
        // TODO: checked for column 5. If data is NULL in the DB, PQgetvalue returns "" and strdup
        // TODO: gets an empty string rather than NULL. Handle consistently with rules_text above.
        c->data       = strdup(PQgetvalue(res, i, 5));
    }

    PQclear(res);
    printf("Loaded %d cards into memory\n", deck->count);
    return true;
}

// TODO: cards_find() is an O(n) linear scan with strcmp. For a small deck (< 100 cards) this is
// TODO: fine, but consider a hash map keyed on card_id if the deck grows or lookups become frequent.
Card *cards_find(Deck *deck, const char *card_id) {
    if (!deck || !card_id) return NULL;

    for (int i = 0; i < deck->count; i++) {
        if (strcmp(deck->cards[i].card_id, card_id) == 0) {
            return &deck->cards[i];
        }
    }

    return NULL;
}

void cards_free(Deck *deck) {
    if (!deck) return;

    for (int i = 0; i < deck->count; i++) {
        Card *c = &deck->cards[i];
        free(c->card_id);
        free(c->name);
        free(c->type);
        free(c->rules_text);
        free(c->data);
    }

    free(deck->cards);
    memset(deck, 0, sizeof(Deck));
}
