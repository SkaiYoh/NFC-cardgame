#ifndef CARD_ACTION_H
#define CARD_ACTION_H

#include "cards.h"
#include <stdbool.h>

/* Forward declare — full game state will grow over time */
typedef struct GameState GameState;

/* Handler signature: receives the card being played and game state */
typedef void (*CardPlayFn)(const Card *card, GameState *state);

/* Register a handler for a card type (e.g. "Spell", "Minion").
 * Max 32 registered types. */
void card_action_register(const char *type, CardPlayFn fn);

/* Play a card: looks up card->type in the registry and calls its handler.
 * Returns false if no handler is registered for the card's type. */
bool card_action_play(const Card *card, GameState *state);

/* Register all built-in card type handlers. Call once at startup. */
void card_action_init(void);

#endif /* CARD_ACTION_H */
