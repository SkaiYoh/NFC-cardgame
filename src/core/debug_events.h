//
// Debug event ring buffer — neutral module consumed by rendering overlay.
// Gameplay code emits events; the overlay draws them.
//

#ifndef NFC_CARDGAME_DEBUG_EVENTS_H
#define NFC_CARDGAME_DEBUG_EVENTS_H

#include <stdbool.h>

#define DEBUG_EVENT_CAPACITY 32

typedef enum {
    DEBUG_EVT_STATE_CHANGE = 0,
    DEBUG_EVT_HIT          = 1,
    DEBUG_EVT_DEATH_FINISH = 2,
} DebugEventType;

typedef struct {
    float posX, posY;
    DebugEventType type;
    float remaining;  // seconds left before expiry
    float duration;   // original duration (for alpha fade calculation)
} DebugFlash;

// Emit a new debug flash event at the given position.
void debug_event_emit_xy(float x, float y, DebugEventType type);

// Tick all active events (call once per frame with deltaTime).
void debug_events_tick(float dt);

// Get the event buffer and count for iteration.
// Returns pointer to the internal ring buffer array (DEBUG_EVENT_CAPACITY entries).
// Callers should skip entries where remaining <= 0.
const DebugFlash *debug_events_buffer(void);

// Reset all events (e.g., on match restart).
void debug_events_clear(void);

#endif //NFC_CARDGAME_DEBUG_EVENTS_H
