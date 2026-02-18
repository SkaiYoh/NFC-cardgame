//
// Created by Nathan Davis on 2/16/26.
//

#include "card_effects.h"
#include "../entities/troop.h"
#include "../entities/entities.h"
#include "../systems/player.h"
#include "../../lib/cJSON.h"
#include <stdio.h>
#include <string.h>

#define MAX_HANDLERS 32

typedef struct {
    const char *type;
    CardPlayFn  play;
} CardHandler;

static CardHandler handlers[MAX_HANDLERS];
static int handler_count = 0;

void card_action_register(const char *type, CardPlayFn fn) {
    if (handler_count >= MAX_HANDLERS) {
        fprintf(stderr, "[card_action] handler registry full, cannot register '%s'\n", type);
        return;
    }
    handlers[handler_count].type = type;
    handlers[handler_count].play = fn;
    handler_count++;
}

bool card_action_play(const Card *card, GameState *state) {
    if (!card || !card->type) return false;

    for (int i = 0; i < handler_count; i++) {
        if (strcmp(handlers[i].type, card->type) == 0) {
            handlers[i].play(card, state);
            return true;
        }
    }

    printf("[PLAY] Unknown card type '%s' for card '%s'\n", card->type, card->name);
    return false;
}

// Helper: spawn a troop from a card for the current player
static void spawn_troop_from_card(const Card *card, GameState *state) {
    if (!state) {
        printf("[%s] %s (cost %d): no game state, skipping spawn\n",
               card->type, card->name, card->cost);
        return;
    }

    int pi = state->currentPlayerIndex;
    Player *player = &state->players[pi];

    // Find first available slot to spawn at
    Vector2 spawnPos = player_center(player);
    for (int i = 0; i < NUM_CARD_SLOTS; i++) {
        if (player_slot_is_available(player, i)) {
            spawnPos = player_slot_spawn_pos(player, i);
            break;
        }
    }

    TroopData data = troop_create_data_from_card(card);
    Entity *e = troop_spawn(player, &data, spawnPos, &state->spriteAtlas);
    if (e) {
        player_add_entity(player, e);
    }
}

static void play_spell(const Card *card, GameState *state) {
    (void)state;
    printf("[SPELL] %s (cost %d): ", card->name, card->cost);

    if (!card->data) {
        printf("no data\n");
        return;
    }

    cJSON *root = cJSON_Parse(card->data);
    if (!root) {
        printf("invalid data\n");
        return;
    }

    cJSON *damage = cJSON_GetObjectItem(root, "damage");
    cJSON *element = cJSON_GetObjectItem(root, "element");
    cJSON *targets = cJSON_GetObjectItem(root, "targets");

    if (damage && cJSON_IsNumber(damage)) {
        printf("would deal %d", damage->valueint);
        if (element && cJSON_IsString(element))
            printf(" %s", element->valuestring);
        printf(" damage");
    }

    if (targets && cJSON_IsArray(targets)) {
        printf(" to [");
        cJSON *t = NULL;
        int first = 1;
        cJSON_ArrayForEach(t, targets) {
            if (cJSON_IsString(t)) {
                printf("%s%s", first ? "" : ", ", t->valuestring);
                first = 0;
            }
        }
        printf("]");
    }

    printf("\n");
    cJSON_Delete(root);
}

static void play_knight(const Card *card, GameState *state) {
    spawn_troop_from_card(card, state);
}

static void play_healer(const Card *card, GameState *state) {
    spawn_troop_from_card(card, state);
}

static void play_assassin(const Card *card, GameState *state) {
    spawn_troop_from_card(card, state);
}

static void play_brute(const Card *card, GameState *state) {
    spawn_troop_from_card(card, state);
}

static void play_farmer(const Card *card, GameState *state) {
    spawn_troop_from_card(card, state);
}

void card_action_init(void) {
    handler_count = 0;
    card_action_register("spell", play_spell);
    card_action_register("knight", play_knight);
    card_action_register("healer", play_healer);
    card_action_register("assassin", play_assassin);
    card_action_register("brute", play_brute);
    card_action_register("farmer", play_farmer);
}
